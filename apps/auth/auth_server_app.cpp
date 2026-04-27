// 代码规范落地：服务入口层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "apps/auth/auth_server_app.h"

#include "runtime/foundation/config/storage_boundary_validation.h"
#include "runtime/foundation/log/logger.h"
#include "runtime/protocol/adapter_utils.h"
#include "runtime/protocol/proto_mapper.h"

#include "game_backend.pb.h"

namespace services::auth {

namespace {

std::string NormalizePeerKey(std::string peer_address) {
    for (auto& ch : peer_address) {
        if (ch == ':' || ch == ' ' || ch == '[' || ch == ']') {
            ch = '_';
        }
    }
    return peer_address.empty() ? "unknown" : peer_address;
}

std::string ResolveClientIp(const game_backend::proto::LoginRequest& request,
                            const framework::protocol::HandlerContext& context) {
    if (!request.client_ip().empty()) {
        return request.client_ip();
    }
    return context.peer_address;
}

bool CheckRateLimit(common::redis::RedisClientPool& redis_pool,
                    const std::string& key,
                    int ttl_seconds,
                    std::int64_t limit,
                    std::string* error_message = nullptr) {
    auto redis = redis_pool.Acquire();
    std::int64_t value = 0;
    if (!redis->IncrementWithExpire(key, ttl_seconds, &value, error_message)) {
        return true;
    }
    return value <= limit;
}

void FillProtoSession(const common::model::Session& session, game_backend::proto::LoginResponse* output) {
    if (output == nullptr) {
        return;
    }

    output->set_auth_token(session.session_id);
    output->set_account_id(session.account_id);
    output->set_player_id(session.player_id);
    output->set_expires_at_epoch_seconds(session.expires_at_epoch_seconds);
}

common::net::Packet BuildLoginResponsePacket(const framework::protocol::HandlerContext& context,
                                             const login_server::LoginResponse& result) {
    game_backend::proto::LoginResponse response;
    common::net::RequestContext response_context = context.request;
    response_context.auth_token = result.session.session_id;
    response_context.player_id = result.default_player_id;
    response_context.account_id = result.session.account_id;
    framework::protocol::FillResponseContext(response_context, &response);
    response.set_player_id(result.default_player_id);
    response.set_account_id(result.session.account_id);
    FillProtoSession(result.session, &response);

    return common::net::BuildPacket(
        common::net::MessageId::kAuthLoginResponse, context.request.request_id, response);
}

}  // namespace

AuthServerApp::AuthServerApp() : framework::service::ServiceApp("auth_server", "configs/auth_server.conf") {}

// 依赖装配：`BuildDependencies` 负责组装运行组件与边界配置。
bool AuthServerApp::BuildDependencies(std::string* error_message) {
    if (!common::config::ValidateAuthStorageConfig(Config(), error_message)) {
        return false;
    }
    const auto account_mysql_options = common::mysql::ReadReadWritePoolOptions(Config(), "storage.account.mysql.", 4);
    account_writer_mysql_pool_ = std::make_unique<common::mysql::MySqlClientPool>(
        account_mysql_options.writer.connection, account_mysql_options.writer.pool_size);
    const auto player_mysql_options = common::mysql::ReadReadWritePoolOptions(Config(), "storage.player.mysql.", 4);
    player_writer_mysql_pool_ = std::make_unique<common::mysql::MySqlClientPool>(
        player_mysql_options.writer.connection, player_mysql_options.writer.pool_size);
    // 状态读取：`ReadPoolOptionsWithFallback` 负责加载上下文并返回稳定结果。
    const auto redis_options = common::redis::ReadPoolOptionsWithFallback(
        Config(), "storage.session.redis.", "storage.account.redis.", 4);
    session_redis_pool_ = std::make_unique<common::redis::RedisClientPool>(
        redis_options.connection, redis_options.pool_size);
    if (!account_writer_mysql_pool_->Initialize(error_message)) {
        return false;
    }
    if (!player_writer_mysql_pool_->Initialize(error_message)) {
        return false;
    }
    if (!session_redis_pool_->Initialize(error_message)) {
        return false;
    }

    account_repository_ =
        std::make_unique<login_server::auth::MySqlAccountRepository>(*account_writer_mysql_pool_, *player_writer_mysql_pool_);
    session_repository_ = std::make_unique<common::session::RedisSessionStore>(
        *session_redis_pool_, Config().GetInt("storage.session.ttl_seconds", 3600));
    login_service_ = std::make_unique<login_server::LoginService>(*account_repository_, *session_repository_);
    return true;
}

// 依赖装配：`RegisterRoutes` 负责组装运行组件与边界配置。
void AuthServerApp::RegisterRoutes() {
    Routes().Register(common::net::MessageId::kAuthLoginRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandleLoginRequest(context, packet);
                      });
}

// 请求处理：`HandleLoginRequest` 承接边界输入并转发到目标链路。
common::net::Packet AuthServerApp::HandleLoginRequest(const framework::protocol::HandlerContext& context,
                                                      const common::net::Packet& packet) const {
    game_backend::proto::LoginRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(context, packet, "invalid auth login request", &request, &error_response)) {
        return error_response;
    }

    std::string rate_limit_error;
    const auto client_ip = ResolveClientIp(request, context);
    const auto peer_key = NormalizePeerKey(client_ip);
    const auto ip_allowed = CheckRateLimit(*session_redis_pool_,
                                           "rate:login:ip:" + peer_key,
                                           Config().GetInt("security.rate_limit.login.ip.ttl_seconds", 60),
                                           Config().GetInt("security.rate_limit.login.ip.limit", 30),
                                           &rate_limit_error);
    const auto account_allowed = CheckRateLimit(*session_redis_pool_,
                                                "rate:login:account:" + request.account_name(),
                                                Config().GetInt("security.rate_limit.login.account.ttl_seconds", 60),
                                                Config().GetInt("security.rate_limit.login.account.limit", 10),
                                                &rate_limit_error);
    if (!ip_allowed || !account_allowed) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "login rate limited: client_ip=" + client_ip + " peer=" + context.peer_address +
                " account=" + request.account_name());
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kRateLimited, "login rate limited");
    }

    const auto result = login_service_->Login(
        {request.account_name(), request.password(), client_ip, request.device_id()});
    if (!result.success) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "login request failed: client_ip=" + client_ip + " peer=" + context.peer_address +
                " account=" + request.account_name() + " error=" +
                std::string(common::error::ToString(result.error_code)));
        return framework::protocol::BuildErrorResponse(context.request, result.error_code, result.error_message);
    }
    return BuildLoginResponsePacket(context, result);
}

}  // namespace services::auth

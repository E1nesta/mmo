// 代码规范落地：服务入口层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "apps/api_gateway/api_gateway_server_app.h"

#include "runtime/foundation/build/build_info.h"
#include "runtime/foundation/config/storage_boundary_validation.h"
#include "runtime/foundation/error/error_code.h"
#include "runtime/foundation/log/logger.h"
#include "runtime/protocol/proto_codec.h"
#include "runtime/protocol/proto_mapper.h"
#include "runtime/transport/service_options.h"

#include "game_backend.pb.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <type_traits>
#include <utility>

namespace services::api_gateway {

namespace {

std::atomic_bool g_running{true};

void HandleSignal(int /*signal*/) {
    g_running.store(false);
}

std::string ExtractHeader(const framework::http::HttpRequest& request, const std::string& key) {
    const auto iter = request.headers.find(key);
    return iter == request.headers.end() ? std::string{} : iter->second;
}

std::string NormalizeAuthToken(std::string value) {
    static constexpr const char* kBearer = "Bearer ";
    if (value.rfind(kBearer, 0) == 0) {
        value.erase(0, 7);
    }
    return value;
}

std::int64_t NowEpochMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::int64_t NowEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string TrimWhitespace(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.pop_back();
    }
    return value;
}

std::string FirstForwardedAddress(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const auto delimiter = value.find(',');
    if (delimiter == std::string::npos) {
        return TrimWhitespace(value);
    }
    return TrimWhitespace(value.substr(0, delimiter));
}

std::string ResolveClientIp(const framework::http::HttpRequest& request) {
    const auto forwarded_for = FirstForwardedAddress(ExtractHeader(request, "x-forwarded-for"));
    if (!forwarded_for.empty()) {
        return forwarded_for;
    }
    return TrimWhitespace(ExtractHeader(request, "x-real-ip"));
}

std::string ResolveDeviceId(const framework::http::HttpRequest& request) {
    return TrimWhitespace(ExtractHeader(request, "x-device-id"));
}

common::error::ErrorCode MapTransportFailure(framework::transport::TransportFailureCode failure_code) {
    using framework::transport::TransportFailureCode;
    switch (failure_code) {
    case TransportFailureCode::kTimeout:
        return common::error::ErrorCode::kUpstreamTimeout;
    case TransportFailureCode::kNoUpstreamClients:
        return common::error::ErrorCode::kServiceUnavailable;
    case TransportFailureCode::kConnectFailed:
    case TransportFailureCode::kResolveFailed:
        return common::error::ErrorCode::kServiceUnavailable;
    default:
        return common::error::ErrorCode::kBadGateway;
    }
}

void CopyResponseContext(const game_backend::proto::ResponseContext& input, game_backend::proto::ResponseContext* output) {
    if (output == nullptr) {
        return;
    }
    output->CopyFrom(input);
}

void CopyReward(const game_backend::proto::Reward& input, game_backend::proto::Reward* output) {
    if (output == nullptr) {
        return;
    }
    output->set_reward_type(input.reward_type());
    output->set_amount(input.amount());
}

template <typename Proto, typename = void>
struct HasPlayerIdSetter : std::false_type {};

template <typename Proto>
struct HasPlayerIdSetter<Proto,
                         std::void_t<decltype(std::declval<Proto&>().set_player_id(0))>> : std::true_type {};

template <typename Proto>
void SyncPlayerIdIfSupported(Proto* proto, std::int64_t player_id) {
    if (proto == nullptr || player_id == 0) {
        return;
    }
    if constexpr (HasPlayerIdSetter<Proto>::value) {
        proto->set_player_id(player_id);
    }
}

}  // namespace

ApiGatewayServerApp::ApiGatewayServerApp(std::string default_service_name, std::string default_config_path)
    : default_service_name_(std::move(default_service_name)),
      default_config_path_(std::move(default_config_path)) {}

// 服务主流程：`Main` 串联启动、运行与收敛阶段。
int ApiGatewayServerApp::Main(int argc, char* argv[]) {
    // 启动阶段：先解析运行参数并处理纯版本输出分支。
    g_running.store(true);
    const auto options = framework::runtime::ParseServiceOptions(argc, argv, default_service_name_, default_config_path_);
    if (options.show_version) {
        std::cout << common::build::Version() << '\n';
        return 0;
    }

    // 初始化阶段：装配日志身份与基础配置。
    auto& logger = common::log::Logger::Instance();
    logger.SetServiceName(options.service_name);
    if (!config_.LoadFromFile(options.config_path)) {
        logger.LogSync(common::log::LogLevel::kError, "failed to load api gateway config");
        return 1;
    }

    logger.SetServiceName(config_.GetString("service.name", options.service_name));
    logger.SetServiceInstanceId(
        config_.GetString("service.instance_id", config_.GetString("service.name", options.service_name)));
    logger.SetEnvironment(config_.GetString("runtime.environment", "local"));
    logger.SetMinLogLevel(config_.GetString("log.level", "info"));
    logger.SetLogFormat(config_.GetString("log.format", "auto"));

    // 装配阶段：构建上游转发与会话依赖，并注册入口路由。
    std::string error_message;
    if (!BuildDependencies(&error_message)) {
        logger.LogSync(common::log::LogLevel::kError, error_message);
        return 1;
    }

    RegisterRoutes();

    if (options.check_only) {
        logger.LogSync(common::log::LogLevel::kInfo, "api gateway configuration and dependencies check passed");
        Shutdown();
        return 0;
    }

    if (!StartServer(&error_message)) {
        logger.LogSync(common::log::LogLevel::kError, error_message);
        Shutdown();
        return 1;
    }

    // 运行阶段：安装退出信号并进入监听循环。
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    logger.Log(common::log::LogLevel::kInfo,
               "api gateway listening on " + config_.GetString("http.listen.host", "0.0.0.0") + ":" +
                   std::to_string(config_.GetInt("http.listen.port", 8080)));

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // 收敛阶段：统一释放网关依赖并刷盘日志。
    Shutdown();
    logger.Flush();
    logger.Shutdown();
    return 0;
}

// 依赖装配：`BuildDependencies` 负责组装运行组件与边界配置。
bool ApiGatewayServerApp::BuildDependencies(std::string* error_message) {
    if (!common::config::ValidateApiGatewayStorageConfig(config_, error_message)) {
        return false;
    }

    // 输入准备阶段：收敛对内服务地址、超时与连接池配置。
    ApiGatewayForwardExecutor::Options options;
    options.auth = {config_.GetString("upstream.auth.host", "127.0.0.1"),
                    config_.GetInt("upstream.auth.port", 7100),
                    config_.GetInt("upstream.timeout_ms", 3000),
                    config_.GetInt("upstream.pool_size", 2),
                    {}};
    options.player_query = {config_.GetString("upstream.player_query.host", "127.0.0.1"),
                            config_.GetInt("upstream.player_query.port", 7200),
                            config_.GetInt("upstream.timeout_ms", 3000),
                            config_.GetInt("upstream.pool_size", 2),
                            {}};
    options.dungeon_runtime = {config_.GetString("upstream.dungeon_runtime.host", "127.0.0.1"),
                               config_.GetInt("upstream.dungeon_runtime.port", 7300),
                               config_.GetInt("upstream.timeout_ms", 3000),
                               config_.GetInt("upstream.pool_size", 2),
                               {}};
    options.social = {config_.GetString("upstream.social.host", "127.0.0.1"),
                      config_.GetInt("upstream.social.port", 7500),
                      config_.GetInt("upstream.timeout_ms", 3000),
                      config_.GetInt("upstream.pool_size", 2),
                      {}};
    // 状态读取：`ReadPoolOptionsWithFallback` 负责加载上下文并返回稳定结果。
    const auto redis_options = common::redis::ReadPoolOptionsWithFallback(
        config_, "storage.session.redis.", "storage.redis.", 4);
    session_redis_pool_ = std::make_unique<common::redis::RedisClientPool>(
        redis_options.connection, redis_options.pool_size);
    if (!session_redis_pool_->Initialize(error_message)) {
        return false;
    }
    session_store_ = std::make_unique<common::session::RedisSessionStore>(
        *session_redis_pool_, config_.GetInt("storage.session.ttl_seconds", 3600));

    // 收敛阶段：构建对内转发执行器。
    forward_executor_ = std::make_unique<ApiGatewayForwardExecutor>(std::move(options));
    return true;
}

// 依赖装配：`RegisterRoutes` 负责组装运行组件与边界配置。
void ApiGatewayServerApp::RegisterRoutes() {
    // 登录入口组：补齐客户端来源字段并转发认证请求。
    router_.RegisterPost("/api/v1/auth/login",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardHttpProto<game_backend::proto::LoginRequest>(
                                 common::net::MessageId::kAuthLoginRequest,
                                 request,
                                 [&request](game_backend::proto::LoginRequest& proto,
                                            common::net::RequestContext& /*context*/) {
                                     proto.set_client_ip(ResolveClientIp(request));
                                     proto.set_device_id(ResolveDeviceId(request));
                                 });
                         });

    // 玩家读取入口组：统一补齐 player_id 与 request context。
    router_.RegisterPost("/api/v1/player/init",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardHttpProto<game_backend::proto::LoadPlayerRequest>(
                                 common::net::MessageId::kPlayerInitRequest,
                                 request,
                                 [](game_backend::proto::LoadPlayerRequest& proto, common::net::RequestContext& context) {
                                     context.player_id = proto.player_id();
                                     common::net::FillProto(context, proto.mutable_context());
                                 });
                         });
    router_.RegisterPost("/api/v1/player/snapshot",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardHttpProto<game_backend::proto::PlayerSnapshotRequest>(
                                 common::net::MessageId::kPlayerSnapshotRequest,
                                 request,
                                 [](game_backend::proto::PlayerSnapshotRequest& proto,
                                    common::net::RequestContext& context) {
                                     context.player_id = proto.player_id();
                                     common::net::FillProto(context, proto.mutable_context());
                                 });
                         });

    // 副本入战入口组：同时支持通用入口与模式映射入口。
    router_.RegisterPost("/api/v1/dungeon/enter",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardHttpProto<game_backend::proto::EnterDungeonRequest>(
                                 common::net::MessageId::kEnterDungeonRequest,
                                 request,
                                 [](game_backend::proto::EnterDungeonRequest& proto,
                                    common::net::RequestContext& context) {
                                     context.player_id = proto.player_id();
                                     common::net::FillProto(context, proto.mutable_context());
                                 });
                         });
    router_.RegisterPost("/api/v1/dungeon/drama/enter",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardMappedHttpProto<game_backend::proto::DungeonDramaEnterRequest,
                                            game_backend::proto::EnterDungeonRequest,
                                            game_backend::proto::EnterDungeonResponse,
                                            game_backend::proto::DungeonDramaEnterResponse>(
            common::net::MessageId::kEnterDungeonRequest,
            request,
            [](const game_backend::proto::DungeonDramaEnterRequest& external,
               common::net::RequestContext& context,
               game_backend::proto::EnterDungeonRequest& internal) {
                context.player_id = external.player_id();
                common::net::FillProto(context, internal.mutable_context());
                internal.set_player_id(external.player_id());
                internal.set_stage_id(external.stage_id());
                internal.set_mode("drama");
                internal.set_loadout_id(external.loadout_id());
                for (const auto& item : external.use_item_list()) {
                    auto* internal_item = internal.add_use_item_list();
                    internal_item->set_item_id(item.item_id());
                    internal_item->set_amount(item.amount());
                }
            },
            [](const game_backend::proto::DungeonDramaEnterRequest&,
               const game_backend::proto::EnterDungeonResponse& internal,
               game_backend::proto::DungeonDramaEnterResponse& external) {
                CopyResponseContext(internal.context(), external.mutable_context());
                external.set_session_id(internal.session_id());
                external.set_remain_stamina(internal.remain_stamina());
                external.set_seed(internal.seed());
                external.set_settle_token(internal.settle_token());
            });
                         });
    router_.RegisterPost("/api/v1/dungeon/general/enter",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardMappedHttpProto<game_backend::proto::DungeonGeneralEnterRequest,
                                            game_backend::proto::EnterDungeonRequest,
                                            game_backend::proto::EnterDungeonResponse,
                                            game_backend::proto::DungeonGeneralEnterResponse>(
            common::net::MessageId::kEnterDungeonRequest,
            request,
            [](const game_backend::proto::DungeonGeneralEnterRequest& external,
               common::net::RequestContext& context,
               game_backend::proto::EnterDungeonRequest& internal) {
                context.player_id = external.player_id();
                common::net::FillProto(context, internal.mutable_context());
                internal.set_player_id(external.player_id());
                internal.set_stage_id(external.stage_id());
                internal.set_mode("general");
                internal.set_loadout_id(external.loadout_id());
                for (const auto& item : external.use_item_list()) {
                    auto* internal_item = internal.add_use_item_list();
                    internal_item->set_item_id(item.item_id());
                    internal_item->set_amount(item.amount());
                }
            },
            [](const game_backend::proto::DungeonGeneralEnterRequest&,
               const game_backend::proto::EnterDungeonResponse& internal,
               game_backend::proto::DungeonGeneralEnterResponse& external) {
                CopyResponseContext(internal.context(), external.mutable_context());
                external.set_session_id(internal.session_id());
                external.set_remain_stamina(internal.remain_stamina());
                external.set_seed(internal.seed());
                external.set_settle_token(internal.settle_token());
            });
                         });

    // 副本结算入口组：支持通用结算与模式映射结算。
    router_.RegisterPost("/api/v1/dungeon/settle",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardHttpProto<game_backend::proto::SettleDungeonRequest>(
                                 common::net::MessageId::kSettleDungeonRequest,
                                 request,
                                 [](game_backend::proto::SettleDungeonRequest& proto,
                                    common::net::RequestContext& context) {
                                     context.player_id = proto.player_id();
                                     common::net::FillProto(context, proto.mutable_context());
                                 });
                         });
    router_.RegisterPost("/api/v1/dungeon/drama/result",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardMappedHttpProto<game_backend::proto::DungeonDramaResultRequest,
                                            game_backend::proto::SettleDungeonRequest,
                                            game_backend::proto::SettleDungeonResponse,
                                            game_backend::proto::DungeonDramaResultResponse>(
            common::net::MessageId::kSettleDungeonRequest,
            request,
            [](const game_backend::proto::DungeonDramaResultRequest& external,
               common::net::RequestContext& context,
               game_backend::proto::SettleDungeonRequest& internal) {
                context.player_id = external.player_id();
                common::net::FillProto(context, internal.mutable_context());
                internal.set_player_id(external.player_id());
                internal.set_session_id(external.session_id());
                internal.set_stage_id(external.stage_id());
                internal.set_star(external.star());
                internal.set_result_code(external.result_code());
                internal.set_client_score(external.client_score());
                internal.set_settle_token(external.settle_token());
                if (external.has_battle_stats()) {
                    internal.mutable_battle_stats()->CopyFrom(external.battle_stats());
                }
            },
            [](const game_backend::proto::DungeonDramaResultRequest&,
               const game_backend::proto::SettleDungeonResponse& internal,
               game_backend::proto::DungeonDramaResultResponse& external) {
                CopyResponseContext(internal.context(), external.mutable_context());
                external.set_reward_grant_id(internal.reward_grant_id());
                external.set_grant_status(internal.grant_status());
                external.set_detail_hydrated(false);
                for (const auto& reward : internal.reward_preview()) {
                    CopyReward(reward, external.add_reward_preview());
                }
            });
                         });
    router_.RegisterPost("/api/v1/dungeon/general/result",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardMappedHttpProto<game_backend::proto::DungeonGeneralResultRequest,
                                            game_backend::proto::SettleDungeonRequest,
                                            game_backend::proto::SettleDungeonResponse,
                                            game_backend::proto::DungeonGeneralResultResponse>(
            common::net::MessageId::kSettleDungeonRequest,
            request,
            [](const game_backend::proto::DungeonGeneralResultRequest& external,
               common::net::RequestContext& context,
               game_backend::proto::SettleDungeonRequest& internal) {
                context.player_id = external.player_id();
                common::net::FillProto(context, internal.mutable_context());
                internal.set_player_id(external.player_id());
                internal.set_session_id(external.session_id());
                internal.set_stage_id(external.stage_id());
                internal.set_star(external.star());
                internal.set_result_code(external.result_code());
                internal.set_client_score(external.client_score());
                internal.set_settle_token(external.settle_token());
                if (external.has_battle_stats()) {
                    internal.mutable_battle_stats()->CopyFrom(external.battle_stats());
                }
            },
            [](const game_backend::proto::DungeonGeneralResultRequest&,
               const game_backend::proto::SettleDungeonResponse& internal,
               game_backend::proto::DungeonGeneralResultResponse& external) {
                CopyResponseContext(internal.context(), external.mutable_context());
                external.set_reward_grant_id(internal.reward_grant_id());
                external.set_grant_status(internal.grant_status());
                external.set_daily_limit(0);
                external.set_detail_hydrated(false);
                for (const auto& reward : internal.reward_preview()) {
                    CopyReward(reward, external.add_reward_preview());
                }
                for (const auto& item : internal.use_item_list()) {
                    auto* output = external.add_use_item_list();
                    output->set_item_id(item.item_id());
                    output->set_amount(item.amount());
                }
            });
                         });

    // 副本查询入口组：活跃副本与奖励发放状态查询。
    router_.RegisterPost("/api/v1/dungeon/active",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardHttpProto<game_backend::proto::GetActiveDungeonRequest>(
                                 common::net::MessageId::kGetActiveDungeonRequest,
                                 request,
                                 [](game_backend::proto::GetActiveDungeonRequest& proto,
                                    common::net::RequestContext& context) {
                                     context.player_id = proto.player_id();
                                     common::net::FillProto(context, proto.mutable_context());
                                 });
                         });
    router_.RegisterPost("/api/v1/dungeon/grant-status",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardHttpProto<game_backend::proto::GetSettlementGrantStatusRequest>(
                                 common::net::MessageId::kGetSettlementGrantStatusRequest,
                                 request,
                                 [](game_backend::proto::GetSettlementGrantStatusRequest& proto,
                                    common::net::RequestContext& context) {
                                     context.player_id = proto.player_id();
                                     common::net::FillProto(context, proto.mutable_context());
                                 });
                         });

    // 社交入口组：统一透传到 social 服务。
    router_.RegisterPost("/api/v1/social/friends/list",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardHttpProto<game_backend::proto::ListFriendsRequest>(
                                 common::net::MessageId::kListFriendsRequest,
                                 request,
                                 [](game_backend::proto::ListFriendsRequest& proto,
                                    common::net::RequestContext& context) {
                                     context.player_id = proto.player_id();
                                     common::net::FillProto(context, proto.mutable_context());
                                 });
                         });
    router_.RegisterPost("/api/v1/social/chat/conversations",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardHttpProto<game_backend::proto::ListConversationsRequest>(
                                 common::net::MessageId::kListConversationsRequest,
                                 request,
                                 [](game_backend::proto::ListConversationsRequest& proto,
                                    common::net::RequestContext& context) {
                                     context.player_id = proto.player_id();
                                     common::net::FillProto(context, proto.mutable_context());
                                 });
                         });
    router_.RegisterPost("/api/v1/social/chat/history",
                         [this](const framework::http::HttpRequest& request) {
        return this->ForwardHttpProto<game_backend::proto::GetChatHistoryRequest>(
                                 common::net::MessageId::kGetChatHistoryRequest,
                                 request,
                                 [](game_backend::proto::GetChatHistoryRequest& proto,
                                    common::net::RequestContext& context) {
                                     context.player_id = proto.player_id();
                                     common::net::FillProto(context, proto.mutable_context());
                                 });
                         });
}

bool ApiGatewayServerApp::StartServer(std::string* error_message) {
    // 启动阶段：根据配置构建 HTTP 服务并绑定路由表。
    server_ = std::make_unique<framework::http::HttpServer>(
        config_.GetString("http.listen.host", "0.0.0.0"),
        config_.GetInt("http.listen.port", 8080));
    return server_->Start(&router_, error_message);
}

// 服务主流程：`Shutdown` 串联启动、运行与收敛阶段。
void ApiGatewayServerApp::Shutdown() {
    // 收敛阶段：先停入口服务，再释放转发与会话依赖。
    if (server_ != nullptr) {
        server_->Shutdown();
        server_.reset();
    }
    session_store_.reset();
    session_redis_pool_.reset();
    forward_executor_.reset();
}

common::net::RequestContext ApiGatewayServerApp::BuildBaseContext(const framework::http::HttpRequest& request) {
    // 输入准备阶段：统一 request_id、trace_id 与 auth_token。
    common::net::RequestContext context;
    context.request_id = next_request_id_.fetch_add(1);
    context.trace_id = ExtractHeader(request, "x-trace-id");
    if (context.trace_id.empty()) {
        context.trace_id = "api-" + std::to_string(context.request_id);
    }
    context.auth_token = NormalizeAuthToken(ExtractHeader(request, "authorization"));
    if (context.auth_token.empty()) {
        context.auth_token = ExtractHeader(request, "x-auth-token");
    }
    return context;
}

bool ApiGatewayServerApp::RequiresSession(common::net::MessageId message_id) const {
    return message_id != common::net::MessageId::kAuthLoginRequest;
}

// 输入校验：`AuthorizeHttpRequest` 校验关键约束并在失败时快速返回。
bool ApiGatewayServerApp::AuthorizeHttpRequest(common::net::MessageId message_id,
                                               common::net::RequestContext* context,
                                               framework::http::HttpResponse* error_response) const {
    // 登录请求不走会话校验，直接放行到认证服务。
    if (!RequiresSession(message_id)) {
        return true;
    }

    if (context == nullptr || error_response == nullptr) {
        return false;
    }
    if (session_store_ == nullptr) {
        *error_response = BuildProtoErrorResponse(
            *context, common::error::ErrorCode::kServiceUnavailable, "session store unavailable");
        return false;
    }

    if (context->auth_token.empty()) {
        *error_response =
            BuildProtoErrorResponse(*context, common::error::ErrorCode::kSessionInvalid, "missing auth token");
        return false;
    }

    // 会话校验阶段：验证存在性、状态、过期时间与玩家归属。
    const auto session = session_store_->FindById(context->auth_token);
    if (!session.has_value() || session->status != common::model::SessionStatus::kActive ||
        (session->expires_at_epoch_seconds > 0 && session->expires_at_epoch_seconds < NowEpochSeconds())) {
        *error_response = BuildProtoErrorResponse(
            *context, common::error::ErrorCode::kSessionInvalid, "session invalid or expired");
        return false;
    }

    if (context->player_id != 0 && session->player_id != context->player_id) {
        *error_response =
            BuildProtoErrorResponse(*context, common::error::ErrorCode::kSessionInvalid, "session player mismatch");
        return false;
    }

    context->player_id = session->player_id;
    context->account_id = session->account_id;
    return true;
}

framework::http::HttpResponse ApiGatewayServerApp::BuildProtoErrorResponse(
    const common::net::RequestContext& context,
    common::error::ErrorCode error_code,
    std::string error_message) const {
    const auto packet = common::net::BuildErrorPacket(context, error_code, error_message);
    return {200, "application/x-protobuf", packet.body, {{"x-proto-message-id", std::to_string(packet.header.msg_id)}}};
}

framework::http::HttpResponse ApiGatewayServerApp::BuildTransportErrorResponse(
    const common::net::RequestContext& context,
    framework::transport::TransportFailureCode failure_code,
    std::string error_message) const {
    const auto packet = common::net::BuildErrorPacket(context, MapTransportFailure(failure_code), error_message);
    return {200, "application/x-protobuf", packet.body, {{"x-proto-message-id", std::to_string(packet.header.msg_id)}}};
}

// 处理阶段：承接 HTTP protobuf 请求并转发到内部服务。
template <typename RequestProto>
framework::http::HttpResponse ApiGatewayServerApp::ForwardHttpProto(
    common::net::MessageId message_id,
    const framework::http::HttpRequest& request,
    const std::function<void(RequestProto&, common::net::RequestContext&)>& enrich) {
    // 解析阶段：先解析外部 protobuf 请求体。
    RequestProto proto_request;
    if (!proto_request.ParseFromString(request.body)) {
        return {400, "text/plain", "invalid protobuf request", {}};
    }

    // 上下文阶段：构建网关上下文并补齐业务字段。
    auto context = BuildBaseContext(request);
    enrich(proto_request, context);
    framework::http::HttpResponse auth_error;
    if (!AuthorizeHttpRequest(message_id, &context, &auth_error)) {
        return auth_error;
    }
    SyncPlayerIdIfSupported(&proto_request, context.player_id);
    common::net::FillProto(context, proto_request.mutable_context());

    // 转发阶段：签名后调用上游服务，失败时映射为统一传输错误。
    auto packet = common::net::BuildPacket(message_id, context.request_id, proto_request);
    std::string sign_error;
    if (!common::net::SignTrustedRequest(message_id, NowEpochMs(), config_.GetString("security.trusted_gateway.shared_secret", "local-dev-gateway-shared-secret"), &packet, &sign_error)) {
        return {500, "text/plain", sign_error, {}};
    }

    common::net::Packet response_packet;
    std::string error_message;
    framework::transport::TransportFailureCode failure_code = framework::transport::TransportFailureCode::kNone;
    if (!forward_executor_->SendAndReceive(message_id, packet, &response_packet, &error_message, &failure_code)) {
        return BuildTransportErrorResponse(context, failure_code, std::move(error_message));
    }

    const auto actual_message_id = common::net::MessageIdFromInt(response_packet.header.msg_id);
    const auto expected_message_id = common::net::ExpectedResponseMessageId(message_id);
    if (actual_message_id.has_value() && *actual_message_id != common::net::MessageId::kErrorResponse &&
        (!expected_message_id.has_value() || *actual_message_id != *expected_message_id)) {
        return BuildTransportErrorResponse(context,
                                           framework::transport::TransportFailureCode::kProtocolDecodeFailed,
                                           "unexpected upstream response type");
    }

    // 返回阶段：原样透传内部 protobuf 响应体。
    return {200,
            "application/x-protobuf",
            response_packet.body,
            {{"x-proto-message-id", std::to_string(response_packet.header.msg_id)}}};
}

template <typename ExternalRequestProto,
          typename InternalRequestProto,
          typename InternalResponseProto,
          typename ExternalResponseProto>
// 处理阶段：承接外部协议并映射为内部协议后转发。
framework::http::HttpResponse ApiGatewayServerApp::ForwardMappedHttpProto(
    common::net::MessageId message_id,
    const framework::http::HttpRequest& request,
    const std::function<void(const ExternalRequestProto&, common::net::RequestContext&, InternalRequestProto&)>& map_request,
    const std::function<void(const ExternalRequestProto&, const InternalResponseProto&, ExternalResponseProto&)>& map_response) {
    // 解析阶段：先解析外部协议请求。
    ExternalRequestProto external_request;
    if (!external_request.ParseFromString(request.body)) {
        return {400, "text/plain", "invalid protobuf request", {}};
    }

    // 映射阶段：外部请求先映射为内部请求，再做统一授权与上下文补齐。
    auto context = BuildBaseContext(request);
    InternalRequestProto internal_request;
    map_request(external_request, context, internal_request);
    framework::http::HttpResponse auth_error;
    if (!AuthorizeHttpRequest(message_id, &context, &auth_error)) {
        return auth_error;
    }
    SyncPlayerIdIfSupported(&internal_request, context.player_id);
    common::net::FillProto(context, internal_request.mutable_context());

    // 转发阶段：签名后调用上游，失败映射为传输错误。
    auto packet = common::net::BuildPacket(message_id, context.request_id, internal_request);
    std::string sign_error;
    if (!common::net::SignTrustedRequest(message_id,
                                         NowEpochMs(),
                                         config_.GetString("security.trusted_gateway.shared_secret",
                                                           "local-dev-gateway-shared-secret"),
                                         &packet,
                                         &sign_error)) {
        return {500, "text/plain", sign_error, {}};
    }

    common::net::Packet response_packet;
    std::string error_message;
    framework::transport::TransportFailureCode failure_code = framework::transport::TransportFailureCode::kNone;
    if (!forward_executor_->SendAndReceive(message_id, packet, &response_packet, &error_message, &failure_code)) {
        return BuildTransportErrorResponse(context, failure_code, std::move(error_message));
    }

    const auto actual_message_id = common::net::MessageIdFromInt(response_packet.header.msg_id);
    const auto expected_message_id = common::net::ExpectedResponseMessageId(message_id);
    if (actual_message_id.has_value() && *actual_message_id == common::net::MessageId::kErrorResponse) {
        return {200,
                "application/x-protobuf",
                response_packet.body,
                {{"x-proto-message-id", std::to_string(response_packet.header.msg_id)}}};
    }
    if (actual_message_id.has_value() &&
        (!expected_message_id.has_value() || *actual_message_id != *expected_message_id)) {
        return BuildTransportErrorResponse(context,
                                           framework::transport::TransportFailureCode::kProtocolDecodeFailed,
                                           "unexpected upstream response type");
    }

    // 收敛阶段：解析内部响应并映射回外部协议响应。
    InternalResponseProto internal_response;
    if (!internal_response.ParseFromString(response_packet.body)) {
        return BuildTransportErrorResponse(context,
                                           framework::transport::TransportFailureCode::kProtocolDecodeFailed,
                                           "invalid upstream protobuf response");
    }

    ExternalResponseProto external_response;
    map_response(external_request, internal_response, external_response);
    std::string external_body;
    if (!external_response.SerializeToString(&external_body)) {
        return {500, "text/plain", "failed to serialize protobuf response", {}};
    }

    // 返回阶段：输出外部协议响应体。
    return {200,
            "application/x-protobuf",
            external_body,
            {{"x-proto-message-id", std::to_string(response_packet.header.msg_id)}}};
}

}  // namespace services::api_gateway

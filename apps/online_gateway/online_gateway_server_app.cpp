// 代码规范落地：服务入口层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "apps/online_gateway/online_gateway_server_app.h"

#include "runtime/foundation/config/storage_boundary_validation.h"
#include "runtime/foundation/log/logger.h"
#include "runtime/protocol/adapter_utils.h"
#include "runtime/protocol/proto_codec.h"
#include "runtime/protocol/proto_mapper.h"
#include "runtime/transport/tls_options.h"

#include "game_backend.pb.h"

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace services::online_gateway {

namespace {

std::string ResolveAuthToken(const game_backend::proto::GateLoginRequest& request) {
    if (!request.auth_token().empty()) {
        return request.auth_token();
    }
    if (!request.session_id().empty()) {
        return request.session_id();
    }
    return request.token();
}

std::string ResolveAuthToken(const game_backend::proto::GateRelinkRequest& request) {
    if (!request.auth_token().empty()) {
        return request.auth_token();
    }
    return request.session_id();
}

std::string NormalizeKey(std::string value) {
    for (auto& ch : value) {
        if (ch == ':' || ch == ' ' || ch == '[' || ch == ']') {
            ch = '_';
        }
    }
    return value.empty() ? "unknown" : value;
}

std::string PlayerPresenceKey(std::int64_t player_id) {
    return "gate:presence:player:" + std::to_string(player_id);
}

std::int64_t CurrentEpochMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string ResolveAdvertiseHost(const common::config::SimpleConfig& config) {
    const auto configured = config.GetString("service.advertise.host");
    if (!configured.empty()) {
        return configured;
    }

    const auto listen_host = config.GetString("service.listen.host", "0.0.0.0");
    if (!listen_host.empty() && listen_host != "0.0.0.0" && listen_host != "::" && listen_host != "[::]") {
        return listen_host;
    }

    return config.GetString("service.instance_id", "online-gateway-local");
}

int ResolveAdvertisePort(const common::config::SimpleConfig& config) {
    const auto configured = config.GetInt("service.advertise.port", 0);
    if (configured > 0) {
        return configured;
    }
    return config.GetInt("service.listen.port", 7000);
}

std::pair<std::string, std::string> ResolveGateRateLimitSubject(const game_backend::proto::GateLoginRequest& request,
                                                                const std::string& peer_address) {
    if (!request.device_id().empty()) {
        return {"device", request.device_id()};
    }
    return {"peer", peer_address};
}

std::pair<std::string, std::string> ResolveGateRateLimitSubject(const game_backend::proto::GateRelinkRequest& request,
                                                                const std::string& peer_address) {
    if (!request.device_id().empty()) {
        return {"device", request.device_id()};
    }
    return {"peer", peer_address};
}

common::net::Packet BuildGateLoginResponsePacket(const framework::protocol::HandlerContext& context,
                                                 int line_no) {
    game_backend::proto::GateLoginResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    response.set_success(true);
    response.set_player_id(context.request.player_id);
    response.set_session_id(context.request.auth_token);
    response.set_line_no(line_no);
    response.set_connection_id(context.connection_id);
    return common::net::BuildPacket(common::net::MessageId::kGateLoginResponse, context.request.request_id, response);
}

common::net::Packet BuildGateRelinkResponsePacket(const framework::protocol::HandlerContext& context,
                                                  int line_no) {
    game_backend::proto::GateRelinkResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    response.set_success(true);
    response.set_player_id(context.request.player_id);
    response.set_session_id(context.request.auth_token);
    response.set_line_no(line_no);
    response.set_connection_id(context.connection_id);
    return common::net::BuildPacket(
        common::net::MessageId::kGateRelinkResponse, context.request.request_id, response);
}

common::net::Packet BuildPublishGateNotificationResponsePacket(const framework::protocol::HandlerContext& context,
                                                               bool player_online,
                                                               bool delivered) {
    game_backend::proto::PublishGateNotificationResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    response.set_player_online(player_online);
    response.set_delivered(delivered);
    return common::net::BuildPacket(
        common::net::MessageId::kPublishGateNotificationResponse, context.request.request_id, response);
}

common::net::Packet BuildKickPlayerResponsePacket(const framework::protocol::HandlerContext& context,
                                                  bool player_online,
                                                  bool delivered) {
    game_backend::proto::KickPlayerResponse response;
    framework::protocol::FillResponseContext(context.request, &response);
    response.set_player_online(player_online);
    response.set_delivered(delivered);
    return common::net::BuildPacket(
        common::net::MessageId::kKickPlayerResponse, context.request.request_id, response);
}

}  // namespace

OnlineGatewayServerApp::OnlineGatewayServerApp()
    : framework::service::ServiceApp("online_gateway_server", "configs/online_gateway_server.conf") {}

// 依赖装配：`BuildDependencies` 负责组装运行组件与边界配置。
bool OnlineGatewayServerApp::BuildDependencies(std::string* error_message) {
    if (!common::config::ValidateOnlineGatewayStorageConfig(Config(), error_message)) {
        return false;
    }
    advertise_host_ = ResolveAdvertiseHost(Config());
    advertise_port_ = ResolveAdvertisePort(Config());
    presence_ttl_seconds_ = std::max(
        30,
        Config().GetInt("online.presence.ttl_seconds",
                        std::max(60, (Config().GetInt("transport.idle_timeout_ms", 30000) / 1000) * 3)));
    peer_forward_timeout_ms_ = std::max(100, Config().GetInt("online.forward.timeout_ms", 1000));
    peer_forward_pool_size_ = std::max(1, Config().GetInt("online.forward.pool_size", 1));
    peer_gateway_tls_options_ = framework::transport::ReadTlsOptions(Config(), "upstream.tls.");
    if (!Config().Contains("upstream.tls.enabled")) {
        peer_gateway_tls_options_.enabled = Config().GetBool("transport.tls.enabled", false);
    }
    if (peer_gateway_tls_options_.ca_file.empty()) {
        peer_gateway_tls_options_.ca_file = Config().GetString("transport.tls.ca_file");
    }
    // 状态读取：`ReadPoolOptionsWithFallback` 负责加载上下文并返回稳定结果。
    const auto redis_options = common::redis::ReadPoolOptionsWithFallback(
        Config(), "storage.session.redis.", "storage.redis.", 4);
    session_redis_pool_ = std::make_unique<common::redis::RedisClientPool>(
        redis_options.connection, redis_options.pool_size);
    if (!session_redis_pool_->Initialize(error_message)) {
        return false;
    }
    session_store_ = std::make_unique<common::session::RedisSessionStore>(
        *session_redis_pool_, Config().GetInt("storage.session.ttl_seconds", 3600));
    session_binding_service_ = std::make_unique<services::gateway::SessionBindingService>(*session_store_);
    return true;
}

// 依赖装配：`RegisterRoutes` 负责组装运行组件与边界配置。
void OnlineGatewayServerApp::RegisterRoutes() {
    Routes().Register(common::net::MessageId::kGatePingRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandleGatePingRequest(context, packet);
                      });
    Routes().Register(common::net::MessageId::kGateLoginRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandleGateLoginRequest(context, packet);
                      });
    Routes().Register(common::net::MessageId::kGateRelinkRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandleGateRelinkRequest(context, packet);
                      });
    Routes().Register(
        common::net::MessageId::kPublishGateNotificationRequest,
        [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
            return HandlePublishGateNotificationRequest(context, packet);
        });
    Routes().Register(common::net::MessageId::kKickPlayerRequest,
                      [this](const framework::protocol::HandlerContext& context, const common::net::Packet& packet) {
                          return HandleKickPlayerRequest(context, packet);
                      });
}

// 请求处理：`OnDisconnect` 承接边界输入并转发到目标链路。
void OnlineGatewayServerApp::OnDisconnect(std::uint64_t connection_id) {
    if (session_binding_service_ != nullptr) {
        const auto binding = session_binding_service_->FindByConnectionId(connection_id);
        session_binding_service_->Unbind(connection_id);
        if (binding.has_value()) {
            (void)DeletePlayerPresenceIfOwned(binding->player_id, connection_id);
        }
    }
}

// 请求处理：`HandleGatePingRequest` 承接边界输入并转发到目标链路。
common::net::Packet OnlineGatewayServerApp::HandleGatePingRequest(const framework::protocol::HandlerContext& context,
                                                                  const common::net::Packet& packet) const {
    game_backend::proto::PingRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(context, packet, "invalid gate ping request", &request, &error_response)) {
        return error_response;
    }
    if (session_binding_service_ != nullptr) {
        const auto status = session_binding_service_->ValidateBoundSession(context.connection_id);
        if (status.status == services::gateway::SessionBindingService::Status::kInvalid) {
            common::log::Logger::Instance().Log(
                common::log::LogLevel::kWarn,
                "gate ping rejected: connection_id=" + std::to_string(context.connection_id) + " reason=" +
                    status.reason);
            (void)DisconnectConnection(context.connection_id);
            return framework::protocol::BuildErrorResponse(
                context.request, common::error::ErrorCode::kSessionInvalid, status.reason);
        }
        if (session_binding_service_->Touch(context.connection_id)) {
            if (const auto binding = session_binding_service_->FindByConnectionId(context.connection_id); binding.has_value()) {
                (void)RefreshPlayerPresence(binding->player_id, context.connection_id);
            }
        }
    }
    return common::net::BuildPingResponsePacket(context.request, request.message());
}

// 请求处理：`HandleGateLoginRequest` 承接边界输入并转发到目标链路。
common::net::Packet OnlineGatewayServerApp::HandleGateLoginRequest(const framework::protocol::HandlerContext& context,
                                                                   const common::net::Packet& packet) {
    game_backend::proto::GateLoginRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(context, packet, "invalid gate login request", &request, &error_response)) {
        return error_response;
    }
    const auto rate_limit_subject = ResolveGateRateLimitSubject(request, context.peer_address);
    if (!CheckGateRateLimit(rate_limit_subject.first, rate_limit_subject.second) ||
        !CheckGateRateLimit("session", NormalizeKey(ResolveAuthToken(request)))) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate login rate limited: subject_bucket=" + rate_limit_subject.first + " subject=" +
                NormalizeKey(rate_limit_subject.second) + " player_id=" +
                std::to_string(request.player_id()));
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kRateLimited, "gate login rate limited");
    }
    GateClientMetadata metadata;
    metadata.line_no = request.line_no();
    metadata.anti_addi = request.anti_addi();
    metadata.device_id = request.device_id();
    metadata.client_session_id = request.session_id();
    return CompleteSessionBind(
        context, ResolveAuthToken(request), metadata, common::net::MessageId::kGateLoginResponse);
}

// 请求处理：`HandleGateRelinkRequest` 承接边界输入并转发到目标链路。
common::net::Packet OnlineGatewayServerApp::HandleGateRelinkRequest(const framework::protocol::HandlerContext& context,
                                                                    const common::net::Packet& packet) {
    game_backend::proto::GateRelinkRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(context, packet, "invalid gate relink request", &request, &error_response)) {
        return error_response;
    }
    const auto rate_limit_subject = ResolveGateRateLimitSubject(request, context.peer_address);
    if (!CheckGateRateLimit(rate_limit_subject.first, rate_limit_subject.second) ||
        !CheckGateRateLimit("session", NormalizeKey(ResolveAuthToken(request)))) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate relink rate limited: subject_bucket=" + rate_limit_subject.first + " subject=" +
                NormalizeKey(rate_limit_subject.second) + " player_id=" +
                std::to_string(request.player_id()));
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kRateLimited, "gate relink rate limited");
    }
    GateClientMetadata metadata;
    metadata.line_no = request.line_no();
    metadata.device_id = request.device_id();
    metadata.client_session_id = request.session_id();
    return CompleteSessionBind(
        context, ResolveAuthToken(request), metadata, common::net::MessageId::kGateRelinkResponse);
}

// 请求处理：`HandlePublishGateNotificationRequest` 承接边界输入并转发到目标链路。
common::net::Packet OnlineGatewayServerApp::HandlePublishGateNotificationRequest(
    const framework::protocol::HandlerContext& context,
    const common::net::Packet& packet) const {
    game_backend::proto::PublishGateNotificationRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(
            context, packet, "invalid publish gate notification request", &request, &error_response)) {
        return error_response;
    }
    if (!ValidateTrustedGateCommand(
            common::net::MessageId::kPublishGateNotificationRequest, context, packet, &error_response)) {
        return error_response;
    }
    const auto delivery = DeliverNotificationRequest(context, request, packet);
    common::log::Logger::Instance().Log(
        delivery.player_online ? (delivery.delivered ? common::log::LogLevel::kInfo : common::log::LogLevel::kWarn)
                               : common::log::LogLevel::kInfo,
        "gate notification publish: player_id=" + std::to_string(request.player_id()) + " type=" +
            std::to_string(request.type()) + " online=" + std::string(delivery.player_online ? "true" : "false") +
            " delivered=" + std::string(delivery.delivered ? "true" : "false"));
    return BuildPublishGateNotificationResponsePacket(context, delivery.player_online, delivery.delivered);
}

// 请求处理：`HandleKickPlayerRequest` 承接边界输入并转发到目标链路。
common::net::Packet OnlineGatewayServerApp::HandleKickPlayerRequest(
    const framework::protocol::HandlerContext& context,
    const common::net::Packet& packet) const {
    game_backend::proto::KickPlayerRequest request;
    common::net::Packet error_response;
    if (!framework::protocol::ParseProtoRequest(
            context, packet, "invalid kick player request", &request, &error_response)) {
        return error_response;
    }
    if (!ValidateTrustedGateCommand(common::net::MessageId::kKickPlayerRequest, context, packet, &error_response)) {
        return error_response;
    }
    const auto delivery = DeliverKickRequest(context, request, packet);
    common::log::Logger::Instance().Log(
        delivery.player_online ? (delivery.delivered ? common::log::LogLevel::kInfo : common::log::LogLevel::kWarn)
                               : common::log::LogLevel::kInfo,
        "gate kick publish: player_id=" + std::to_string(request.player_id()) + " online=" +
            std::string(delivery.player_online ? "true" : "false") +
            " delivered=" + std::string(delivery.delivered ? "true" : "false"));
    return BuildKickPlayerResponsePacket(context, delivery.player_online, delivery.delivered);
}

common::net::Packet OnlineGatewayServerApp::CompleteSessionBind(const framework::protocol::HandlerContext& context,
                                                                const std::string& auth_token,
                                                                const GateClientMetadata& metadata,
                                                                common::net::MessageId response_message_id) {
    auto mutable_context = context;
    mutable_context.request.auth_token = auth_token;
    const auto result =
        session_binding_service_->ValidateOrRestore(mutable_context.connection_id, &mutable_context.request);
    if (result.status == services::gateway::SessionBindingService::Status::kInvalid) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate session bind failed: connection_id=" + std::to_string(context.connection_id) + " reason=" +
                result.reason);
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kSessionInvalid, result.reason);
    }
    const auto session = session_store_ != nullptr ? session_store_->FindById(auth_token) : std::nullopt;
    if (!session.has_value() || session->status != common::model::SessionStatus::kActive) {
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kSessionInvalid, "session is not active");
    }
    if (!session->device_id.empty() && metadata.device_id.empty()) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate device missing: player_id=" + std::to_string(session->player_id) + " session_id=" + auth_token);
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kSessionInvalid, "device_id missing");
    }
    if (!session->device_id.empty() && !metadata.device_id.empty() && session->device_id != metadata.device_id) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate device mismatch: player_id=" + std::to_string(session->player_id) + " session_id=" + auth_token +
                " bound_device=" + session->device_id + " request_device=" + metadata.device_id);
        return framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kSessionInvalid, "device_id mismatch");
    }
    if (session->device_id.empty() && !metadata.device_id.empty() && session_store_ != nullptr) {
        (void)session_store_->BindDeviceId(auth_token, metadata.device_id);
    }
    session_binding_service_->UpdateClientMetadata(mutable_context.connection_id,
                                                   metadata.line_no,
                                                   metadata.anti_addi,
                                                   metadata.device_id,
                                                   metadata.client_session_id);
    const auto binding = session_binding_service_->FindByConnectionId(mutable_context.connection_id);
    if (binding.has_value()) {
        (void)RefreshPlayerPresence(binding->player_id, mutable_context.connection_id);
    }
    const auto line_no = binding.has_value() ? binding->line_no : metadata.line_no;
    common::log::Logger::Instance().Log(
        common::log::LogLevel::kInfo,
        std::string(response_message_id == common::net::MessageId::kGateRelinkResponse ? "gate relink success: "
                                                                                        : "gate login success: ") +
            "player_id=" + std::to_string(mutable_context.request.player_id) + " connection_id=" +
            std::to_string(mutable_context.connection_id) + " line_no=" + std::to_string(line_no));
    if (response_message_id == common::net::MessageId::kGateRelinkResponse) {
        return BuildGateRelinkResponsePacket(mutable_context, line_no);
    }
    return BuildGateLoginResponsePacket(mutable_context, line_no);
}

bool OnlineGatewayServerApp::RefreshPlayerPresence(std::int64_t player_id, std::uint64_t connection_id) const {
    if (session_redis_pool_ == nullptr || player_id <= 0 || connection_id == 0 || advertise_host_.empty() || advertise_port_ <= 0) {
        return false;
    }

    auto redis = session_redis_pool_->Acquire();
    std::string error_message;
    if (!redis->HSet(PlayerPresenceKey(player_id),
                     {{"instance_id", Config().GetString("service.instance_id", "online-gateway-local")},
                      {"host", advertise_host_},
                      {"port", std::to_string(advertise_port_)},
                      {"connection_id", std::to_string(connection_id)},
                      {"updated_at_ms", std::to_string(CurrentEpochMs())}},
                     presence_ttl_seconds_,
                     &error_message)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate presence refresh failed: player_id=" + std::to_string(player_id) + " connection_id=" +
                std::to_string(connection_id) + " error=" + error_message);
        return false;
    }

    return true;
}

bool OnlineGatewayServerApp::DeletePlayerPresenceIfOwned(std::int64_t player_id, std::uint64_t connection_id) const {
    if (session_redis_pool_ == nullptr || player_id <= 0 || connection_id == 0) {
        return false;
    }

    auto redis = session_redis_pool_->Acquire();
    std::string error_message;
    const auto presence = redis->HGetAll(PlayerPresenceKey(player_id), &error_message);
    if (!presence.has_value()) {
        if (!error_message.empty()) {
            common::log::Logger::Instance().Log(
                common::log::LogLevel::kWarn,
                "gate presence delete lookup failed: player_id=" + std::to_string(player_id) + " error=" + error_message);
            return false;
        }
        return true;
    }

    const auto expected_instance_id = Config().GetString("service.instance_id", "online-gateway-local");
    const auto instance_iter = presence->find("instance_id");
    const auto connection_iter = presence->find("connection_id");
    if (instance_iter == presence->end() || connection_iter == presence->end() ||
        instance_iter->second != expected_instance_id || connection_iter->second != std::to_string(connection_id)) {
        return true;
    }

    if (!redis->Del(PlayerPresenceKey(player_id), &error_message)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate presence delete failed: player_id=" + std::to_string(player_id) + " connection_id=" +
                std::to_string(connection_id) + " error=" + error_message);
        return false;
    }

    return true;
}

// 状态读取：`FindPlayerPresence` 负责加载上下文并返回稳定结果。
std::optional<OnlineGatewayServerApp::PlayerPresenceRoute> OnlineGatewayServerApp::FindPlayerPresence(
    std::int64_t player_id) const {
    if (session_redis_pool_ == nullptr || player_id <= 0) {
        return std::nullopt;
    }

    auto redis = session_redis_pool_->Acquire();
    std::string error_message;
    const auto presence = redis->HGetAll(PlayerPresenceKey(player_id), &error_message);
    if (!presence.has_value()) {
        if (!error_message.empty()) {
            common::log::Logger::Instance().Log(
                common::log::LogLevel::kWarn,
                "gate presence lookup failed: player_id=" + std::to_string(player_id) + " error=" + error_message);
        }
        return std::nullopt;
    }

    const auto instance_iter = presence->find("instance_id");
    const auto host_iter = presence->find("host");
    const auto port_iter = presence->find("port");
    const auto connection_iter = presence->find("connection_id");
    if (instance_iter == presence->end() || host_iter == presence->end() || port_iter == presence->end() ||
        connection_iter == presence->end() || instance_iter->second.empty() || host_iter->second.empty()) {
        return std::nullopt;
    }

    PlayerPresenceRoute route;
    route.instance_id = instance_iter->second;
    route.host = host_iter->second;
    try {
        route.port = std::stoi(port_iter->second);
        route.connection_id = static_cast<std::uint64_t>(std::stoull(connection_iter->second));
        if (const auto updated_iter = presence->find("updated_at_ms"); updated_iter != presence->end()) {
            route.updated_at_ms = std::stoll(updated_iter->second);
        }
    } catch (...) {
        return std::nullopt;
    }

    if (route.port <= 0) {
        return std::nullopt;
    }
    return route;
}

OnlineGatewayServerApp::GateDeliveryResult OnlineGatewayServerApp::DeliverNotificationRequest(
    const framework::protocol::HandlerContext& context,
    const game_backend::proto::PublishGateNotificationRequest& request,
    const common::net::Packet& packet) const {
    (void)context;
    if (session_binding_service_ != nullptr &&
        session_binding_service_->FindConnectionIdByPlayerId(request.player_id()).has_value()) {
        return {true, PushNotificationToPlayer(request.player_id(), request.type(), request.timestamp())};
    }

    const auto route = FindPlayerPresence(request.player_id());
    if (!route.has_value()) {
        return {false, false};
    }

    const auto local_instance_id = Config().GetString("service.instance_id", "online-gateway-local");
    if (route->instance_id == local_instance_id) {
        (void)DeletePlayerPresenceIfOwned(request.player_id(), route->connection_id);
        return {false, false};
    }
    if (request.remote_forwarded()) {
        return {false, false};
    }

    auto forwarded_request = request;
    forwarded_request.set_remote_forwarded(true);
    const auto response_packet = ForwardTrustedRequestToGateway(
        common::net::MessageId::kPublishGateNotificationRequest, forwarded_request, packet.header.request_id, *route);
    if (!response_packet.has_value()) {
        (void)DeletePlayerPresenceIfOwned(request.player_id(), route->connection_id);
        return {false, false};
    }

    game_backend::proto::PublishGateNotificationResponse response;
    if (!common::net::ParseMessage(response_packet->body, &response)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate notification forward decode failed: player_id=" + std::to_string(request.player_id()) +
                " route=" + route->host + ":" + std::to_string(route->port));
        return {true, false};
    }
    if (!response.player_online()) {
        (void)DeletePlayerPresenceIfOwned(request.player_id(), route->connection_id);
    }
    return {response.player_online(), response.delivered()};
}

OnlineGatewayServerApp::GateDeliveryResult OnlineGatewayServerApp::DeliverKickRequest(
    const framework::protocol::HandlerContext& context,
    const game_backend::proto::KickPlayerRequest& request,
    const common::net::Packet& packet) const {
    (void)context;
    if (session_binding_service_ != nullptr &&
        session_binding_service_->FindConnectionIdByPlayerId(request.player_id()).has_value()) {
        return {true, KickPlayer(request.player_id(), request.reason())};
    }

    const auto route = FindPlayerPresence(request.player_id());
    if (!route.has_value()) {
        return {false, false};
    }

    const auto local_instance_id = Config().GetString("service.instance_id", "online-gateway-local");
    if (route->instance_id == local_instance_id) {
        (void)DeletePlayerPresenceIfOwned(request.player_id(), route->connection_id);
        return {false, false};
    }
    if (request.remote_forwarded()) {
        return {false, false};
    }

    auto forwarded_request = request;
    forwarded_request.set_remote_forwarded(true);
    const auto response_packet = ForwardTrustedRequestToGateway(
        common::net::MessageId::kKickPlayerRequest, forwarded_request, packet.header.request_id, *route);
    if (!response_packet.has_value()) {
        (void)DeletePlayerPresenceIfOwned(request.player_id(), route->connection_id);
        return {false, false};
    }

    game_backend::proto::KickPlayerResponse response;
    if (!common::net::ParseMessage(response_packet->body, &response)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate kick forward decode failed: player_id=" + std::to_string(request.player_id()) + " route=" +
                route->host + ":" + std::to_string(route->port));
        return {true, false};
    }
    if (!response.player_online()) {
        (void)DeletePlayerPresenceIfOwned(request.player_id(), route->connection_id);
    }
    return {response.player_online(), response.delivered()};
}

// 请求处理：`ForwardTrustedRequestToGateway` 承接边界输入并转发到目标链路。
std::optional<common::net::Packet> OnlineGatewayServerApp::ForwardTrustedRequestToGateway(
    common::net::MessageId message_id,
    const google::protobuf::MessageLite& request,
    std::uint64_t request_id,
    const PlayerPresenceRoute& route) const {
    auto packet = common::net::BuildPacket(message_id, request_id, request);
    std::string sign_error;
    if (!common::net::SignTrustedRequest(message_id, CurrentEpochMs(), ForwardSharedSecret(), &packet, &sign_error)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate forward signing failed: route=" + route.host + ":" + std::to_string(route.port) +
                " error=" + sign_error);
        return std::nullopt;
    }

    auto& client = ResolvePeerGatewayClient(route);
    common::net::Packet response_packet;
    std::string error_message;
    framework::transport::TransportFailureCode failure_code = framework::transport::TransportFailureCode::kNone;
    if (!client.SendAndReceive(packet, &response_packet, &error_message, &failure_code)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate forward failed: route=" + route.host + ":" + std::to_string(route.port) + " failure_code=" +
                std::to_string(static_cast<int>(failure_code)) + " error=" + error_message);
        return std::nullopt;
    }

    return response_packet;
}

// 状态读取：`ResolvePeerGatewayClient` 负责加载上下文并返回稳定结果。
framework::transport::UpstreamClientPool& OnlineGatewayServerApp::ResolvePeerGatewayClient(
    const PlayerPresenceRoute& route) const {
    auto tls_options = peer_gateway_tls_options_;
    if (tls_options.enabled && tls_options.server_name.empty()) {
        tls_options.server_name = route.host;
    }

    const auto key = route.host + ":" + std::to_string(route.port) + ":" + (tls_options.enabled ? "tls" : "plain") +
                     ":" + tls_options.server_name;
    std::lock_guard lock(peer_gateway_clients_mutex_);
    auto& client = peer_gateway_clients_[key];
    if (client == nullptr) {
        client = std::make_unique<framework::transport::UpstreamClientPool>(
            route.host, route.port, peer_forward_timeout_ms_, peer_forward_pool_size_, tls_options);
    }
    return *client;
}

// 请求处理：`ForwardSharedSecret` 承接边界输入并转发到目标链路。
std::string OnlineGatewayServerApp::ForwardSharedSecret() const {
    return Config().GetString("security.gate.shared_secret",
                              Config().GetString("security.trusted_gateway.shared_secret",
                                                 "local-dev-gate-shared-secret"));
}

bool OnlineGatewayServerApp::PushNotificationToPlayer(std::int64_t player_id, int type, std::int64_t timestamp) const {
    if (session_binding_service_ == nullptr) {
        return false;
    }
    const auto connection_id = session_binding_service_->FindConnectionIdByPlayerId(player_id);
    if (!connection_id.has_value()) {
        return false;
    }

    common::net::RequestContext context;
    context.request_id = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                        std::chrono::system_clock::now().time_since_epoch())
                                                        .count());
    context.player_id = player_id;
    if (const auto binding = session_binding_service_->FindByConnectionId(*connection_id); binding.has_value()) {
        context.auth_token = binding->auth_token;
        context.account_id = binding->account_id;
    }
    return SendPacketToConnection(*connection_id, common::net::BuildGateNotificationPacket(context, type, timestamp));
}

bool OnlineGatewayServerApp::KickPlayer(std::int64_t player_id, const std::string& reason) const {
    if (session_binding_service_ == nullptr) {
        return false;
    }
    const auto connection_id = session_binding_service_->FindConnectionIdByPlayerId(player_id);
    if (!connection_id.has_value()) {
        return false;
    }

    common::net::RequestContext context;
    context.request_id = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                        std::chrono::system_clock::now().time_since_epoch())
                                                        .count());
    context.player_id = player_id;
    if (const auto binding = session_binding_service_->FindByConnectionId(*connection_id); binding.has_value()) {
        context.auth_token = binding->auth_token;
        context.account_id = binding->account_id;
        if (session_store_ != nullptr) {
            (void)session_store_->RevokeById(binding->auth_token);
        }
    }

    const auto sent = SendPacketToConnection(*connection_id, common::net::BuildGateKickPacket(context, reason));
    common::log::Logger::Instance().Log(
        common::log::LogLevel::kInfo,
        "gate kick: player_id=" + std::to_string(player_id) + " connection_id=" + std::to_string(*connection_id) +
            " reason=" + reason);
    return sent && DisconnectConnection(*connection_id);
}

// 输入校验：`ValidateTrustedGateCommand` 校验关键约束并在失败时快速返回。
bool OnlineGatewayServerApp::ValidateTrustedGateCommand(common::net::MessageId message_id,
                                                        const framework::protocol::HandlerContext& context,
                                                        const common::net::Packet& packet,
                                                        common::net::Packet* error_response) const {
    std::string error_message;
    const auto max_clock_skew_ms =
        static_cast<std::int64_t>(std::max(0, Config().GetInt("security.gate.max_clock_skew_ms",
                                                              Config().GetInt("security.trusted_gateway.max_clock_skew_ms",
                                                                              10000))));
    const auto shared_secret = Config().GetString("security.gate.shared_secret",
                                                  Config().GetString("security.trusted_gateway.shared_secret",
                                                                     "local-dev-gate-shared-secret"));
    if (common::net::ValidateTrustedRequest(message_id, max_clock_skew_ms, shared_secret, packet, &error_message)) {
        return true;
    }
    common::log::Logger::Instance().Log(
        common::log::LogLevel::kWarn,
        "trusted gate command validation failed: message_id=" + std::to_string(static_cast<int>(message_id)) +
            " connection_id=" + std::to_string(context.connection_id) + " reason=" + error_message);
    if (error_response != nullptr) {
        *error_response = framework::protocol::BuildErrorResponse(
            context.request, common::error::ErrorCode::kTrustedGatewayInvalid, error_message);
    }
    return false;
}

// 输入校验：`CheckGateRateLimit` 校验关键约束并在失败时快速返回。
bool OnlineGatewayServerApp::CheckGateRateLimit(const std::string& bucket, const std::string& subject) const {
    if (session_redis_pool_ == nullptr) {
        return true;
    }
    auto redis = session_redis_pool_->Acquire();
    std::string error_message;
    std::int64_t value = 0;
    const auto ttl_seconds = Config().GetInt("security.rate_limit.gate.ttl_seconds", 10);
    const auto limit = static_cast<std::int64_t>(Config().GetInt("security.rate_limit.gate.limit", 10));
    if (!redis->IncrementWithExpire("rate:gate:" + bucket + ":" + NormalizeKey(subject), ttl_seconds, &value, &error_message)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "gate rate limiter storage unavailable: " + error_message);
        return true;
    }
    return value <= limit;
}

}  // namespace services::online_gateway

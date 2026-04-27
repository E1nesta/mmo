// 代码规范落地：服务入口层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "apps/gateway/session_binding_service.h"
#include "runtime/session/redis_session_store.h"
#include "runtime/storage/redis/redis_client_pool.h"
#include "runtime/transport/service_app.h"
#include "runtime/transport/transport_client.h"

#include "game_backend.pb.h"

#include <google/protobuf/message_lite.h>

#include <mutex>
#include <unordered_map>

namespace services::online_gateway {

class OnlineGatewayServerApp : public framework::service::ServiceApp {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    OnlineGatewayServerApp();

// 受保护扩展：为派生类保留可控扩展点。
protected:
    bool BuildDependencies(std::string* error_message) override;
    void RegisterRoutes() override;
    void OnDisconnect(std::uint64_t connection_id) override;
    bool SignsTrustedGatewayRequests() const override { return true; }

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    struct GateClientMetadata {
        int line_no = 0;
        int anti_addi = 0;
        std::string device_id;
        std::string client_session_id;
    };

    struct PlayerPresenceRoute {
        std::string instance_id;
        std::string host;
        int port = 0;
        std::uint64_t connection_id = 0;
        std::int64_t updated_at_ms = 0;
    };

    struct GateDeliveryResult {
        bool player_online = false;
        bool delivered = false;
    };

    common::net::Packet HandleGatePingRequest(const framework::protocol::HandlerContext& context,
                                              const common::net::Packet& packet) const;
    common::net::Packet HandleGateLoginRequest(const framework::protocol::HandlerContext& context,
                                               const common::net::Packet& packet);
    common::net::Packet HandleGateRelinkRequest(const framework::protocol::HandlerContext& context,
                                                const common::net::Packet& packet);
    common::net::Packet HandlePublishGateNotificationRequest(const framework::protocol::HandlerContext& context,
                                                             const common::net::Packet& packet) const;
    common::net::Packet HandleKickPlayerRequest(const framework::protocol::HandlerContext& context,
                                                const common::net::Packet& packet) const;

    common::net::Packet CompleteSessionBind(const framework::protocol::HandlerContext& context,
                                            const std::string& auth_token,
                                            const GateClientMetadata& metadata,
                                            common::net::MessageId response_message_id);
    bool PushNotificationToPlayer(std::int64_t player_id, int type, std::int64_t timestamp) const;
    bool KickPlayer(std::int64_t player_id, const std::string& reason) const;
    bool ValidateTrustedGateCommand(common::net::MessageId message_id,
                                    const framework::protocol::HandlerContext& context,
                                    const common::net::Packet& packet,
                                    common::net::Packet* error_response) const;
    bool CheckGateRateLimit(const std::string& bucket, const std::string& subject) const;
    bool RefreshPlayerPresence(std::int64_t player_id, std::uint64_t connection_id) const;
    bool DeletePlayerPresenceIfOwned(std::int64_t player_id, std::uint64_t connection_id) const;
    std::optional<PlayerPresenceRoute> FindPlayerPresence(std::int64_t player_id) const;
    GateDeliveryResult DeliverNotificationRequest(const framework::protocol::HandlerContext& context,
                                                  const game_backend::proto::PublishGateNotificationRequest& request,
                                                  const common::net::Packet& packet) const;
    GateDeliveryResult DeliverKickRequest(const framework::protocol::HandlerContext& context,
                                          const game_backend::proto::KickPlayerRequest& request,
                                          const common::net::Packet& packet) const;
    std::optional<common::net::Packet> ForwardTrustedRequestToGateway(common::net::MessageId message_id,
                                                                      const google::protobuf::MessageLite& request,
                                                                      std::uint64_t request_id,
                                                                      const PlayerPresenceRoute& route) const;
    framework::transport::UpstreamClientPool& ResolvePeerGatewayClient(const PlayerPresenceRoute& route) const;
    std::string ForwardSharedSecret() const;

    std::unique_ptr<common::redis::RedisClientPool> session_redis_pool_;
    std::unique_ptr<common::session::RedisSessionStore> session_store_;
    std::unique_ptr<services::gateway::SessionBindingService> session_binding_service_;
    std::string advertise_host_;
    int advertise_port_ = 0;
    int presence_ttl_seconds_ = 120;
    int peer_forward_timeout_ms_ = 1000;
    int peer_forward_pool_size_ = 1;
    framework::transport::TlsOptions peer_gateway_tls_options_;
    mutable std::mutex peer_gateway_clients_mutex_;
    mutable std::unordered_map<std::string, std::unique_ptr<framework::transport::UpstreamClientPool>> peer_gateway_clients_;
};

}  // namespace services::online_gateway

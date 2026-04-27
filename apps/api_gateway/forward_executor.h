// 代码规范落地：服务入口层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/protocol/message_id.h"
#include "runtime/transport/transport_client.h"

namespace services::api_gateway {

class ApiGatewayForwardExecutor {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    struct UpstreamOptions {
        std::string host;
        int port = 0;
        int timeout_ms = 3000;
        int pool_size = 2;
        framework::transport::TlsOptions tls;
    };

    struct Options {
        UpstreamOptions auth;
        UpstreamOptions player_query;
        UpstreamOptions dungeon_runtime;
        UpstreamOptions social;
    };

    explicit ApiGatewayForwardExecutor(Options options);

    bool SendAndReceive(common::net::MessageId message_id,
                        const common::net::Packet& request_packet,
                        common::net::Packet* response_packet,
                        std::string* error_message,
                        framework::transport::TransportFailureCode* failure_code = nullptr);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    framework::transport::UpstreamClientPool& ResolveUpstream(common::net::MessageId message_id);

    std::unique_ptr<framework::transport::UpstreamClientPool> auth_upstream_;
    std::unique_ptr<framework::transport::UpstreamClientPool> player_query_upstream_;
    std::unique_ptr<framework::transport::UpstreamClientPool> dungeon_runtime_upstream_;
    std::unique_ptr<framework::transport::UpstreamClientPool> social_upstream_;
};

}  // namespace services::api_gateway

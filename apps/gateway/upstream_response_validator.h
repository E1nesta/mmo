// 代码规范落地：服务入口层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "apps/gateway/session_binding_service.h"
#include "runtime/foundation/error/error_code.h"
#include "runtime/protocol/message_id.h"
#include "runtime/protocol/packet.h"
#include "runtime/transport/transport_client.h"

namespace framework::protocol {
struct HandlerContext;
}

namespace services::gateway {

class UpstreamResponseValidator {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    explicit UpstreamResponseValidator(SessionBindingService& session_binding_service);

    [[nodiscard]] common::error::ErrorCode MapForwardError(const std::string& error_message) const;
    [[nodiscard]] common::error::ErrorCode MapForwardError(
        framework::transport::TransportFailureCode failure_code,
        const std::string& error_message) const;
    [[nodiscard]] common::net::Packet Validate(common::net::MessageId message_id,
                                               const framework::protocol::HandlerContext& context,
                                               const common::net::Packet& request_packet,
                                               common::net::Packet upstream_response);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    SessionBindingService& session_binding_service_;
};

}  // namespace services::gateway

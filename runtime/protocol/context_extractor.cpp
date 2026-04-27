// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/protocol/context_extractor.h"

#include "runtime/protocol/proto_mapper.h"

namespace framework::protocol {

std::optional<common::net::RequestContext> ExtractRequestContext(common::net::MessageId message_id,
                                                                 const common::net::Packet& packet,
                                                                 std::string* error_message) {
    common::net::RequestContext context;
    if (common::net::ExtractRequestContext(message_id, packet.body, &context)) {
        return context;
    }

    if (error_message != nullptr) {
        *error_message = "failed to parse request context";
    }
    return std::nullopt;
}

}  // namespace framework::protocol

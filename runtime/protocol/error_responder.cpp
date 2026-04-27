// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/protocol/error_responder.h"

#include "runtime/protocol/proto_mapper.h"

namespace framework::protocol {

common::net::Packet BuildErrorResponse(const common::net::RequestContext& context,
                                       common::error::ErrorCode error_code,
                                       const std::string& error_message) {
    return common::net::BuildErrorPacket(context, error_code, error_message);
}

}  // namespace framework::protocol

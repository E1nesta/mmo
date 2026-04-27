// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/error/error_code.h"
#include "runtime/protocol/packet.h"
#include "runtime/protocol/request_context.h"

#include <string>

namespace framework::protocol {

common::net::Packet BuildErrorResponse(const common::net::RequestContext& context,
                                       common::error::ErrorCode error_code,
                                       const std::string& error_message);

}  // namespace framework::protocol

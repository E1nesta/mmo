// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/protocol/message_id.h"
#include "runtime/protocol/handler_context.h"

#include <optional>
#include <string>

namespace framework::execution {

enum class ExecutionKeyKind {
    kDirect,
    kConnection,
    kSession,
    kPlayer,
};

struct ExecutionKey {
    ExecutionKeyKind kind = ExecutionKeyKind::kDirect;
    std::string value;
};

struct RequestExecutionPolicy {
    ExecutionKeyKind key_kind = ExecutionKeyKind::kDirect;
};

std::optional<ExecutionKey> BuildExecutionKey(const RequestExecutionPolicy& policy,
                                              const framework::protocol::HandlerContext& context);
std::string DescribeExecutionKey(const ExecutionKey& key);

}  // namespace framework::execution

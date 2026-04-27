// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/execution/execution_types.h"

namespace framework::execution {

std::optional<ExecutionKey> BuildExecutionKey(const RequestExecutionPolicy& policy,
                                              const framework::protocol::HandlerContext& context) {
    switch (policy.key_kind) {
    case ExecutionKeyKind::kDirect:
        return std::nullopt;
    case ExecutionKeyKind::kConnection:
        return ExecutionKey{policy.key_kind, std::to_string(context.connection_id)};
    case ExecutionKeyKind::kSession:
        if (context.request.auth_token.empty()) {
            return std::nullopt;
        }
        return ExecutionKey{policy.key_kind, context.request.auth_token};
    case ExecutionKeyKind::kPlayer:
        if (context.request.player_id == 0) {
            return std::nullopt;
        }
        return ExecutionKey{policy.key_kind, std::to_string(context.request.player_id)};
    }

    return std::nullopt;
}

std::string DescribeExecutionKey(const ExecutionKey& key) {
    return std::to_string(static_cast<int>(key.kind)) + ":" + key.value;
}

}  // namespace framework::execution

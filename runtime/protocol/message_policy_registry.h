// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/protocol/message_id.h"
#include "runtime/execution/execution_types.h"

#include <optional>

namespace framework::protocol {

struct MessagePolicy {
    bool requires_auth_token = false;
    bool requires_player = false;
    bool allow_player_id_from_body = false;
    framework::execution::ExecutionKeyKind execution_key_kind =
        framework::execution::ExecutionKeyKind::kDirect;
    std::optional<common::net::MessageId> expected_response;
};

class MessagePolicyRegistry {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    static std::optional<MessagePolicy> Find(common::net::MessageId message_id);
};

}  // namespace framework::protocol

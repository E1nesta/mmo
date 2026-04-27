// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/protocol/message_policy_registry.h"

namespace framework::protocol {

// 状态读取：`Find` 负责加载上下文并返回稳定结果。
std::optional<MessagePolicy> MessagePolicyRegistry::Find(common::net::MessageId message_id) {
    using common::net::MessageId;
    using framework::execution::ExecutionKeyKind;

    switch (message_id) {
    case MessageId::kGatePingRequest:
        return MessagePolicy{false, false, false, ExecutionKeyKind::kDirect, MessageId::kGatePongResponse};
    case MessageId::kAuthLoginRequest:
        return MessagePolicy{false, false, false, ExecutionKeyKind::kConnection, MessageId::kAuthLoginResponse};
    case MessageId::kPlayerInitRequest:
        return MessagePolicy{true, true, true, ExecutionKeyKind::kPlayer, MessageId::kPlayerInitResponse};
    case MessageId::kPlayerSnapshotRequest:
        return MessagePolicy{true, true, true, ExecutionKeyKind::kPlayer, MessageId::kPlayerSnapshotResponse};
    case MessageId::kEnterDungeonRequest:
        return MessagePolicy{true, true, true, ExecutionKeyKind::kPlayer, MessageId::kEnterDungeonResponse};
    case MessageId::kSettleDungeonRequest:
        return MessagePolicy{true, true, true, ExecutionKeyKind::kPlayer, MessageId::kSettleDungeonResponse};
    case MessageId::kGetActiveDungeonRequest:
        return MessagePolicy{true, true, true, ExecutionKeyKind::kPlayer, MessageId::kGetActiveDungeonResponse};
    case MessageId::kGetSettlementGrantStatusRequest:
        return MessagePolicy{
            true, true, true, ExecutionKeyKind::kPlayer, MessageId::kGetSettlementGrantStatusResponse};
    case MessageId::kListFriendsRequest:
        return MessagePolicy{true, true, true, ExecutionKeyKind::kPlayer, MessageId::kListFriendsResponse};
    case MessageId::kListConversationsRequest:
        return MessagePolicy{
            true, true, true, ExecutionKeyKind::kPlayer, MessageId::kListConversationsResponse};
    case MessageId::kGetChatHistoryRequest:
        return MessagePolicy{true, true, true, ExecutionKeyKind::kPlayer, MessageId::kGetChatHistoryResponse};
    case MessageId::kGateLoginRequest:
        return MessagePolicy{false, false, false, ExecutionKeyKind::kConnection, MessageId::kGateLoginResponse};
    case MessageId::kGateRelinkRequest:
        return MessagePolicy{false, false, false, ExecutionKeyKind::kConnection, MessageId::kGateRelinkResponse};
    case MessageId::kPublishGateNotificationRequest:
        return MessagePolicy{
            false, false, false, ExecutionKeyKind::kDirect, MessageId::kPublishGateNotificationResponse};
    case MessageId::kKickPlayerRequest:
        return MessagePolicy{false, false, false, ExecutionKeyKind::kDirect, MessageId::kKickPlayerResponse};
    default:
        return std::nullopt;
    }
}

}  // namespace framework::protocol

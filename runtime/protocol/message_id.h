// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace common::net {

enum class MessageId : std::uint32_t {
    kGatePingRequest = 1,
    kPingRequest = kGatePingRequest,
    kGatePongResponse = 2,
    kPingResponse = kGatePongResponse,
    kAuthLoginRequest = 1001,
    kLoginRequest = kAuthLoginRequest,
    kAuthLoginResponse = 1002,
    kLoginResponse = kAuthLoginResponse,
    kPlayerInitRequest = 1101,
    kLoadPlayerRequest = kPlayerInitRequest,
    kPlayerInitResponse = 1102,
    kLoadPlayerResponse = kPlayerInitResponse,
    kPlayerSnapshotRequest = 1103,
    kPlayerSnapshotResponse = 1104,
    kEnterDungeonRequest = 1201,
    kEnterDungeonResponse = 1202,
    kSettleDungeonRequest = 1203,
    kSettleDungeonResponse = 1204,
    kGetSettlementGrantStatusRequest = 1205,
    kGetSettlementGrantStatusResponse = 1206,
    kGetActiveDungeonRequest = 1207,
    kGetActiveDungeonResponse = 1208,
    kListFriendsRequest = 1301,
    kListFriendsResponse = 1302,
    kListConversationsRequest = 1303,
    kListConversationsResponse = 1304,
    kGetChatHistoryRequest = 1305,
    kGetChatHistoryResponse = 1306,
    kGateLoginRequest = 2001,
    kGateLoginResponse = 2002,
    kGateRelinkRequest = 2003,
    kGateRelinkResponse = 2004,
    kSocialFriendNotification = 2005,
    kSocialChatPush = 2006,
    kGateKick = 2007,
    kGateNotification = 2008,
    kPublishGateNotificationRequest = 2009,
    kPublishGateNotificationResponse = 2010,
    kKickPlayerRequest = 2011,
    kKickPlayerResponse = 2012,
    kErrorResponse = 9000,
};

inline std::optional<MessageId> MessageIdFromInt(std::uint32_t value) {
    switch (value) {
    case 1:
        return MessageId::kGatePingRequest;
    case 2:
        return MessageId::kGatePongResponse;
    case 1001:
        return MessageId::kAuthLoginRequest;
    case 1002:
        return MessageId::kAuthLoginResponse;
    case 1101:
        return MessageId::kPlayerInitRequest;
    case 1102:
        return MessageId::kPlayerInitResponse;
    case 1103:
        return MessageId::kPlayerSnapshotRequest;
    case 1104:
        return MessageId::kPlayerSnapshotResponse;
    case 1201:
        return MessageId::kEnterDungeonRequest;
    case 1202:
        return MessageId::kEnterDungeonResponse;
    case 1203:
        return MessageId::kSettleDungeonRequest;
    case 1204:
        return MessageId::kSettleDungeonResponse;
    case 1205:
        return MessageId::kGetSettlementGrantStatusRequest;
    case 1206:
        return MessageId::kGetSettlementGrantStatusResponse;
    case 1207:
        return MessageId::kGetActiveDungeonRequest;
    case 1208:
        return MessageId::kGetActiveDungeonResponse;
    case 1301:
        return MessageId::kListFriendsRequest;
    case 1302:
        return MessageId::kListFriendsResponse;
    case 1303:
        return MessageId::kListConversationsRequest;
    case 1304:
        return MessageId::kListConversationsResponse;
    case 1305:
        return MessageId::kGetChatHistoryRequest;
    case 1306:
        return MessageId::kGetChatHistoryResponse;
    case 2001:
        return MessageId::kGateLoginRequest;
    case 2002:
        return MessageId::kGateLoginResponse;
    case 2003:
        return MessageId::kGateRelinkRequest;
    case 2004:
        return MessageId::kGateRelinkResponse;
    case 2005:
        return MessageId::kSocialFriendNotification;
    case 2006:
        return MessageId::kSocialChatPush;
    case 2007:
        return MessageId::kGateKick;
    case 2008:
        return MessageId::kGateNotification;
    case 2009:
        return MessageId::kPublishGateNotificationRequest;
    case 2010:
        return MessageId::kPublishGateNotificationResponse;
    case 2011:
        return MessageId::kKickPlayerRequest;
    case 2012:
        return MessageId::kKickPlayerResponse;
    case 9000:
        return MessageId::kErrorResponse;
    default:
        return std::nullopt;
    }
}

inline std::string_view ToString(MessageId message_id) {
    switch (message_id) {
    case MessageId::kGatePingRequest:
        return "GATE_PING_REQUEST";
    case MessageId::kGatePongResponse:
        return "GATE_PONG_RESPONSE";
    case MessageId::kAuthLoginRequest:
        return "AUTH_LOGIN_REQUEST";
    case MessageId::kAuthLoginResponse:
        return "AUTH_LOGIN_RESPONSE";
    case MessageId::kPlayerInitRequest:
        return "PLAYER_INIT_REQUEST";
    case MessageId::kPlayerInitResponse:
        return "PLAYER_INIT_RESPONSE";
    case MessageId::kPlayerSnapshotRequest:
        return "PLAYER_SNAPSHOT_REQUEST";
    case MessageId::kPlayerSnapshotResponse:
        return "PLAYER_SNAPSHOT_RESPONSE";
    case MessageId::kEnterDungeonRequest:
        return "ENTER_DUNGEON_REQUEST";
    case MessageId::kEnterDungeonResponse:
        return "ENTER_DUNGEON_RESPONSE";
    case MessageId::kSettleDungeonRequest:
        return "SETTLE_DUNGEON_REQUEST";
    case MessageId::kSettleDungeonResponse:
        return "SETTLE_DUNGEON_RESPONSE";
    case MessageId::kGetSettlementGrantStatusRequest:
        return "GET_SETTLEMENT_GRANT_STATUS_REQUEST";
    case MessageId::kGetSettlementGrantStatusResponse:
        return "GET_SETTLEMENT_GRANT_STATUS_RESPONSE";
    case MessageId::kGetActiveDungeonRequest:
        return "GET_ACTIVE_DUNGEON_REQUEST";
    case MessageId::kGetActiveDungeonResponse:
        return "GET_ACTIVE_DUNGEON_RESPONSE";
    case MessageId::kListFriendsRequest:
        return "LIST_FRIENDS_REQUEST";
    case MessageId::kListFriendsResponse:
        return "LIST_FRIENDS_RESPONSE";
    case MessageId::kListConversationsRequest:
        return "LIST_CONVERSATIONS_REQUEST";
    case MessageId::kListConversationsResponse:
        return "LIST_CONVERSATIONS_RESPONSE";
    case MessageId::kGetChatHistoryRequest:
        return "GET_CHAT_HISTORY_REQUEST";
    case MessageId::kGetChatHistoryResponse:
        return "GET_CHAT_HISTORY_RESPONSE";
    case MessageId::kGateLoginRequest:
        return "GATE_LOGIN_REQUEST";
    case MessageId::kGateLoginResponse:
        return "GATE_LOGIN_RESPONSE";
    case MessageId::kGateRelinkRequest:
        return "GATE_RELINK_REQUEST";
    case MessageId::kGateRelinkResponse:
        return "GATE_RELINK_RESPONSE";
    case MessageId::kSocialFriendNotification:
        return "SOCIAL_FRIEND_NOTIFICATION";
    case MessageId::kSocialChatPush:
        return "SOCIAL_CHAT_PUSH";
    case MessageId::kGateKick:
        return "GATE_KICK";
    case MessageId::kGateNotification:
        return "GATE_NOTIFICATION";
    case MessageId::kPublishGateNotificationRequest:
        return "PUBLISH_GATE_NOTIFICATION_REQUEST";
    case MessageId::kPublishGateNotificationResponse:
        return "PUBLISH_GATE_NOTIFICATION_RESPONSE";
    case MessageId::kKickPlayerRequest:
        return "KICK_PLAYER_REQUEST";
    case MessageId::kKickPlayerResponse:
        return "KICK_PLAYER_RESPONSE";
    case MessageId::kErrorResponse:
        return "ERROR_RESPONSE";
    }

    return "UNKNOWN";
}

inline std::optional<MessageId> ExpectedResponseMessageId(MessageId request_id) {
    switch (request_id) {
    case MessageId::kGatePingRequest:
        return MessageId::kGatePongResponse;
    case MessageId::kAuthLoginRequest:
        return MessageId::kAuthLoginResponse;
    case MessageId::kPlayerInitRequest:
        return MessageId::kPlayerInitResponse;
    case MessageId::kPlayerSnapshotRequest:
        return MessageId::kPlayerSnapshotResponse;
    case MessageId::kEnterDungeonRequest:
        return MessageId::kEnterDungeonResponse;
    case MessageId::kSettleDungeonRequest:
        return MessageId::kSettleDungeonResponse;
    case MessageId::kGetSettlementGrantStatusRequest:
        return MessageId::kGetSettlementGrantStatusResponse;
    case MessageId::kGetActiveDungeonRequest:
        return MessageId::kGetActiveDungeonResponse;
    case MessageId::kListFriendsRequest:
        return MessageId::kListFriendsResponse;
    case MessageId::kListConversationsRequest:
        return MessageId::kListConversationsResponse;
    case MessageId::kGetChatHistoryRequest:
        return MessageId::kGetChatHistoryResponse;
    case MessageId::kGateLoginRequest:
        return MessageId::kGateLoginResponse;
    case MessageId::kGateRelinkRequest:
        return MessageId::kGateRelinkResponse;
    case MessageId::kPublishGateNotificationRequest:
        return MessageId::kPublishGateNotificationResponse;
    case MessageId::kKickPlayerRequest:
        return MessageId::kKickPlayerResponse;
    default:
        return std::nullopt;
    }
}

}  // namespace common::net

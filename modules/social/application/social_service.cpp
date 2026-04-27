// 代码规范落地：业务编排层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/social/application/social_service.h"

namespace game_server::social {

SocialService::SocialService(SocialRepository& social_repository) : social_repository_(social_repository) {}

SocialListResponse<FriendSummary> SocialService::ListFriends(std::int64_t player_id) const {
    return {true, common::error::ErrorCode::kOk, "", social_repository_.ListFriends(player_id)};
}

SocialListResponse<ConversationSummary> SocialService::ListConversations(std::int64_t player_id) const {
    return {true, common::error::ErrorCode::kOk, "", social_repository_.ListConversations(player_id)};
}

SocialListResponse<ChatMessage> SocialService::GetChatHistory(std::int64_t player_id,
                                                              const std::string& conversation_id) const {
    return {true, common::error::ErrorCode::kOk, "", social_repository_.GetChatHistory(player_id, conversation_id)};
}

}  // namespace game_server::social

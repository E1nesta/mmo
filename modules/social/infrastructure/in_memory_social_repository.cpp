// 代码规范落地：基础设施层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/social/infrastructure/in_memory_social_repository.h"

namespace game_server::social {

std::vector<FriendSummary> InMemorySocialRepository::ListFriends(std::int64_t /*player_id*/) const {
    return {};
}

std::vector<ConversationSummary> InMemorySocialRepository::ListConversations(std::int64_t /*player_id*/) const {
    return {};
}

std::vector<ChatMessage> InMemorySocialRepository::GetChatHistory(std::int64_t /*player_id*/,
                                                                  const std::string& /*conversation_id*/) const {
    return {};
}

}  // namespace game_server::social

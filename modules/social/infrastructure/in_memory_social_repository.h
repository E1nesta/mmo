// 代码规范落地：基础设施层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/social/ports/social_repository.h"

namespace game_server::social {

class InMemorySocialRepository final : public SocialRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    [[nodiscard]] std::vector<FriendSummary> ListFriends(std::int64_t player_id) const override;
    [[nodiscard]] std::vector<ConversationSummary> ListConversations(std::int64_t player_id) const override;
    [[nodiscard]] std::vector<ChatMessage> GetChatHistory(std::int64_t player_id,
                                                          const std::string& conversation_id) const override;
};

}  // namespace game_server::social

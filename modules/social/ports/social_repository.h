// 代码规范落地：端口契约层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/social/domain/social_models.h"

namespace game_server::social {

class SocialRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    virtual ~SocialRepository() = default;

    [[nodiscard]] virtual std::vector<FriendSummary> ListFriends(std::int64_t player_id) const = 0;
    [[nodiscard]] virtual std::vector<ConversationSummary> ListConversations(std::int64_t player_id) const = 0;
    [[nodiscard]] virtual std::vector<ChatMessage> GetChatHistory(std::int64_t player_id,
                                                                  const std::string& conversation_id) const = 0;
};

}  // namespace game_server::social

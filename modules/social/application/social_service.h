// 代码规范落地：业务编排层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/social/ports/social_repository.h"
#include "runtime/foundation/error/error_code.h"

namespace game_server::social {

template <typename T>
struct SocialListResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    std::vector<T> items;
};

class SocialService {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    explicit SocialService(SocialRepository& social_repository);

    [[nodiscard]] SocialListResponse<FriendSummary> ListFriends(std::int64_t player_id) const;
    [[nodiscard]] SocialListResponse<ConversationSummary> ListConversations(std::int64_t player_id) const;
    [[nodiscard]] SocialListResponse<ChatMessage> GetChatHistory(std::int64_t player_id,
                                                                 const std::string& conversation_id) const;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    SocialRepository& social_repository_;
};

}  // namespace game_server::social

// 代码规范落地：领域模型层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace game_server::social {

struct FriendSummary {
    std::int64_t player_id = 0;
    std::string nickname;
    int level = 0;
    bool online = false;
};

struct ConversationSummary {
    std::string conversation_id;
    std::string title;
    int unread_count = 0;
    std::string last_message_preview;
};

struct ChatMessage {
    std::string message_id;
    std::int64_t sender_player_id = 0;
    std::string content;
    std::int64_t sent_at_epoch_ms = 0;
};

}  // namespace game_server::social

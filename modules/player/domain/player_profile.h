// 代码规范落地：领域模型层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <cstdint>
#include <string>

namespace common::model {

struct PlayerProfile {
    std::int64_t player_id = 0;
    std::int64_t account_id = 0;
    int server_id = 0;
    std::string player_name;
    std::string nickname;
    int level = 1;
    int stamina = 0;
    std::int64_t gold = 0;
    std::int64_t diamond = 0;
    int main_stage_id = 0;
    int main_chapter_id = 0;
    std::int64_t fight_power = 0;
};

}  // namespace common::model

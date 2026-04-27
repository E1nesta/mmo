// 代码规范落地：领域模型层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

namespace game_server::dungeon_runtime {

struct StageConfig {
    int chapter_id = 0;
    int stage_id = 0;
    int required_level = 1;
    int cost_stamina = 10;
    int max_star = 3;
    int normal_gold_reward = 100;
    int first_clear_diamond_reward = 50;
};

}  // namespace game_server::dungeon_runtime

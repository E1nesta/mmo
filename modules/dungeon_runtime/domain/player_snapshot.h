// 代码规范落地：领域模型层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <cstdint>
#include <vector>

namespace game_server::dungeon_runtime {

struct DungeonRoleSummary {
    int role_id = 0;
    int level = 0;
    int star = 0;
};

// 副本运行时所需的最小玩家快照投影。
struct PlayerSnapshot {
    std::int64_t player_id = 0;
    int level = 1;
    int stamina = 0;
    std::vector<DungeonRoleSummary> role_summaries;
};

}  // namespace game_server::dungeon_runtime

// 代码规范落地：领域模型层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/dungeon_runtime/domain/reward.h"

#include <cstdint>
#include <string>
#include <vector>

namespace game_server::dungeon_runtime {

struct DungeonUseItem {
    int item_id = 0;
    int amount = 0;
};

struct DungeonBattleStats {
    int pass_time_seconds = 0;
    int dungeon_rank = 0;
    int master_be_hit = 0;
    int use_ougi = 0;
    int dodge_seconds = 0;
    int heal_sum = 0;
    int be_harm = 0;
    int kill_enemy = 0;
    int master_combo = 0;
    std::string dungeon_report_json;
};

struct DungeonContext {
    std::int64_t session_id = 0;
    std::int64_t player_id = 0;
    int stage_id = 0;
    std::string mode = "pve";
    int loadout_id = 0;
    std::vector<DungeonUseItem> use_item_list;
    int cost_energy = 0;
    int remain_energy_after = 0;
    std::int64_t seed = 0;
    DungeonBattleStats battle_stats;
    bool settled = false;
    std::int64_t reward_grant_id = 0;
    int grant_status = 0;
    std::vector<Reward> rewards;
};

}  // namespace game_server::dungeon_runtime

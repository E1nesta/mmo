// 代码规范落地：领域模型层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/player/domain/currency_balance.h"
#include "modules/player/domain/player_role_summary.h"
#include "modules/player/domain/player_state.h"
#include "modules/player/domain/player_stage_progress.h"

#include <cstdint>
#include <string>
#include <vector>

namespace common::model {

struct HomePlayerDisplay {
    std::int64_t player_id = 0;
    std::int64_t account_id = 0;
    int server_id = 0;
    std::string player_name;
    std::string nickname;
    int level = 1;
};

struct HomeResourceSummary {
    int stamina = 0;
    std::int64_t gold = 0;
    std::int64_t diamond = 0;
    std::vector<CurrencyBalance> currencies;
};

struct HomeCharacterSummary {
    std::int64_t fight_power = 0;
    std::vector<PlayerRoleSummary> role_summaries;
};

struct HomeDungeonSummary {
    int main_stage_id = 0;
    int main_chapter_id = 0;
    std::vector<PlayerStageProgress> stage_progress;
    int stage_progress_count = 0;
};

struct HomeActivitySummary {
    bool hydrated = false;
};

struct HomeEntryStateSummary {
    bool hydrated = false;
};

struct HomeInitView {
    HomePlayerDisplay player_display;
    HomeResourceSummary resource_summary;
    HomeCharacterSummary character_summary;
    HomeDungeonSummary dungeon_summary;
    HomeActivitySummary activity_summary;
    HomeEntryStateSummary entry_state_summary;
};

inline HomeInitView BuildHomeInitView(const PlayerState& player_state) {
    HomeInitView view;
    view.player_display.player_id = player_state.profile.player_id;
    view.player_display.account_id = player_state.profile.account_id;
    view.player_display.server_id = player_state.profile.server_id;
    view.player_display.player_name = player_state.profile.player_name;
    view.player_display.nickname = player_state.profile.nickname;
    view.player_display.level = player_state.profile.level;

    view.resource_summary.stamina = player_state.profile.stamina;
    view.resource_summary.gold = player_state.profile.gold;
    view.resource_summary.diamond = player_state.profile.diamond;
    view.resource_summary.currencies = player_state.currencies;

    view.character_summary.fight_power = player_state.profile.fight_power;
    view.character_summary.role_summaries = player_state.role_summaries;

    view.dungeon_summary.main_stage_id = player_state.profile.main_stage_id;
    view.dungeon_summary.main_chapter_id = player_state.profile.main_chapter_id;
    view.dungeon_summary.stage_progress = player_state.stage_progress;
    view.dungeon_summary.stage_progress_count = static_cast<int>(player_state.stage_progress.size());

    return view;
}

}  // namespace common::model

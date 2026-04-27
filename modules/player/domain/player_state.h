// 代码规范落地：领域模型层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/player/domain/currency_balance.h"
#include "modules/player/domain/player_stage_progress.h"
#include "modules/player/domain/player_profile.h"
#include "modules/player/domain/player_role_summary.h"

#include <vector>

namespace common::model {

struct PlayerState {
    PlayerProfile profile;
    std::vector<PlayerStageProgress> stage_progress;
    std::vector<CurrencyBalance> currencies;
    std::vector<PlayerRoleSummary> role_summaries;
};

}  // namespace common::model

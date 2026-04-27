// 代码规范落地：业务编排层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/error/error_code.h"
#include "modules/player/domain/home_init_view.h"
#include "modules/player/domain/reward.h"
#include "modules/player/ports/player_cache_repository.h"
#include "modules/player/domain/player_profile.h"
#include "modules/player/ports/player_repository.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace game_server::player {

// 应用结果：统一返回业务阶段输出。
struct LoadPlayerResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    common::model::HomeInitView home_init;
    bool loaded_from_cache = false;
};

struct PlayerSnapshotResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    bool found = false;
    std::int64_t player_id = 0;
    int level = 0;
    int stamina = 0;
    std::string nickname;
    std::int64_t gold = 0;
    std::int64_t diamond = 0;
    int main_stage_id = 0;
    int main_chapter_id = 0;
    std::int64_t fight_power = 0;
    std::vector<common::model::CurrencyBalance> currencies;
    std::vector<common::model::PlayerRoleSummary> role_summaries;
    int stage_progress_count = 0;
};

struct BattleEntrySnapshotResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    bool found = false;
    std::int64_t player_id = 0;
    int level = 0;
    int energy = 0;
    std::vector<common::model::PlayerRoleSummary> role_summaries;
};

struct InvalidatePlayerCacheResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
};

struct PrepareBattleEntryResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    int remain_energy = 0;
};

struct CancelBattleEntryResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
};

struct ApplyRewardGrantResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    std::vector<common::model::CurrencyBalance> applied_currencies;
};

// 应用服务：编排流程阶段并收敛状态变化。
class PlayerService {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    PlayerService(PlayerRepository& player_repository,
                  PlayerCacheRepository& player_cache_repository);

    [[nodiscard]] LoadPlayerResponse LoadPlayer(std::int64_t player_id);
    [[nodiscard]] PlayerSnapshotResponse GetPlayerSnapshot(std::int64_t player_id);
    [[nodiscard]] BattleEntrySnapshotResponse GetBattleEntrySnapshot(std::int64_t player_id);
    [[nodiscard]] InvalidatePlayerCacheResponse InvalidatePlayerCache(std::int64_t player_id);
    [[nodiscard]] PrepareBattleEntryResponse PrepareBattleEntry(std::int64_t player_id,
                                                                std::int64_t session_id,
                                                                int energy_cost,
                                                                const std::string& idempotency_key);
    [[nodiscard]] CancelBattleEntryResponse CancelBattleEntry(std::int64_t player_id,
                                                              std::int64_t session_id,
                                                              int energy_refund,
                                                              const std::string& idempotency_key);
    [[nodiscard]] ApplyRewardGrantResponse ApplyRewardGrant(std::int64_t player_id,
                                                            std::int64_t grant_id,
                                                            std::int64_t session_id,
                                                            const std::vector<common::model::Reward>& rewards,
                                                            const std::string& idempotency_key);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] std::optional<common::model::PlayerState> LoadCachedPlayer(std::int64_t player_id) const;
    [[nodiscard]] std::optional<common::model::PlayerState> LoadPlayerFromStorage(std::int64_t player_id) const;
    [[nodiscard]] LoadPlayerResponse BuildLoadSuccess(const common::model::PlayerState& player_state,
                                                      bool loaded_from_cache) const;
    void RefreshPlayerCacheBestEffort(std::int64_t player_id) const;

    PlayerRepository& player_repository_;
    PlayerCacheRepository& player_cache_repository_;
};

}  // namespace game_server::player

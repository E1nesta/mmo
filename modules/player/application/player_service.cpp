// 代码规范落地：业务编排层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/player/application/player_service.h"

#include "runtime/foundation/log/logger.h"

namespace game_server::player {

namespace {

LoadPlayerResponse BuildLoadPlayerError(common::error::ErrorCode error_code, std::string error_message) {
    return {false, error_code, std::move(error_message), {}, false};
}

LoadPlayerResponse BuildLoadPlayerSuccess(const common::model::PlayerState& player_state, bool loaded_from_cache) {
    return {true, common::error::ErrorCode::kOk, "", common::model::BuildHomeInitView(player_state), loaded_from_cache};
}

PlayerSnapshotResponse BuildSnapshotSuccess(const common::model::PlayerState& player_state) {
    return {true,
            common::error::ErrorCode::kOk,
            "",
            true,
            player_state.profile.player_id,
            player_state.profile.level,
            player_state.profile.stamina,
            player_state.profile.nickname,
            player_state.profile.gold,
            player_state.profile.diamond,
            player_state.profile.main_stage_id,
            player_state.profile.main_chapter_id,
            player_state.profile.fight_power,
            player_state.currencies,
            player_state.role_summaries,
            static_cast<int>(player_state.stage_progress.size())};
}

BattleEntrySnapshotResponse BuildBattleEntrySnapshotSuccess(std::int64_t player_id,
                                                            int level,
                                                            int energy,
                                                            std::vector<common::model::PlayerRoleSummary> role_summaries) {
    return {true, common::error::ErrorCode::kOk, "", true, player_id, level, energy, std::move(role_summaries)};
}

BattleEntrySnapshotResponse BuildBattleEntrySnapshotMissing() {
    return {true, common::error::ErrorCode::kOk, "", false, 0, 0, 0, {}};
}

PlayerSnapshotResponse BuildSnapshotMissing() {
    return {true, common::error::ErrorCode::kOk, "", false, 0, 0, 0, "", 0, 0, 0, 0, 0, {}, {}, 0};
}

InvalidatePlayerCacheResponse BuildInvalidateSuccess() {
    return {true, common::error::ErrorCode::kOk, ""};
}

InvalidatePlayerCacheResponse BuildInvalidateFailure(common::error::ErrorCode error_code, std::string error_message) {
    return {false, error_code, std::move(error_message)};
}

common::error::ErrorCode MapMutationError(PlayerMutationError error) {
    switch (error) {
    case PlayerMutationError::kPlayerNotFound:
        return common::error::ErrorCode::kPlayerNotFound;
    case PlayerMutationError::kStaminaNotEnough:
        return common::error::ErrorCode::kStaminaNotEnough;
    case PlayerMutationError::kBattleMismatch:
        return common::error::ErrorCode::kBattleMismatch;
    case PlayerMutationError::kAlreadyApplied:
        return common::error::ErrorCode::kBattleAlreadySettled;
    case PlayerMutationError::kStorageFailure:
        return common::error::ErrorCode::kStorageError;
    case PlayerMutationError::kNone:
        break;
    }
    return common::error::ErrorCode::kOk;
}

}  // namespace

PlayerService::PlayerService(PlayerRepository& player_repository,
                             PlayerCacheRepository& player_cache_repository)
    : player_repository_(player_repository),
      player_cache_repository_(player_cache_repository) {}

// 状态读取：`LoadPlayer` 负责加载上下文并返回稳定结果。
LoadPlayerResponse PlayerService::LoadPlayer(std::int64_t player_id) {
    if (const auto cached_state = LoadCachedPlayer(player_id); cached_state.has_value()) {
        return BuildLoadSuccess(*cached_state, true);
    }

    const auto player_state = LoadPlayerFromStorage(player_id);
    if (!player_state.has_value()) {
        return BuildLoadPlayerError(common::error::ErrorCode::kPlayerNotFound, "player not found");
    }

    player_cache_repository_.Save(*player_state);
    return BuildLoadSuccess(*player_state, false);
}

// 状态读取：`GetPlayerSnapshot` 负责加载上下文并返回稳定结果。
PlayerSnapshotResponse PlayerService::GetPlayerSnapshot(std::int64_t player_id) {
    if (const auto cached_state = LoadCachedPlayer(player_id); cached_state.has_value()) {
        return BuildSnapshotSuccess(*cached_state);
    }

    const auto player_state = LoadPlayerFromStorage(player_id);
    if (!player_state.has_value()) {
        return BuildSnapshotMissing();
    }

    player_cache_repository_.Save(*player_state);
    return BuildSnapshotSuccess(*player_state);
}

// 状态读取：`GetBattleEntrySnapshot` 负责加载上下文并返回稳定结果。
BattleEntrySnapshotResponse PlayerService::GetBattleEntrySnapshot(std::int64_t player_id) {
    const auto result = player_repository_.GetBattleEntrySnapshot(player_id);
    if (!result.success) {
        return {false, MapMutationError(result.error), result.error_message, false, 0, 0, 0, {}};
    }
    if (!result.found) {
        return BuildBattleEntrySnapshotMissing();
    }
    return BuildBattleEntrySnapshotSuccess(player_id, result.level, result.energy, std::move(result.role_summaries));
}

// 状态推进：`InvalidatePlayerCache` 执行写链或补偿并收敛状态变化。
InvalidatePlayerCacheResponse PlayerService::InvalidatePlayerCache(std::int64_t player_id) {
    if (player_cache_repository_.Invalidate(player_id)) {
        return BuildInvalidateSuccess();
    }

    return BuildInvalidateFailure(common::error::ErrorCode::kStorageError, "failed to invalidate player cache");
}

// 状态推进：`PrepareBattleEntry` 执行写链或补偿并收敛状态变化。
PrepareBattleEntryResponse PlayerService::PrepareBattleEntry(std::int64_t player_id,
                                                             std::int64_t session_id,
                                                             int energy_cost,
                                                             const std::string& idempotency_key) {
    const auto result = player_repository_.PrepareBattleEntry(player_id, session_id, energy_cost, idempotency_key);
    if (!result.success) {
        return {false, MapMutationError(result.error), result.error_message, 0};
    }

    RefreshPlayerCacheBestEffort(player_id);
    return {true, common::error::ErrorCode::kOk, "", result.remain_energy};
}

CancelBattleEntryResponse PlayerService::CancelBattleEntry(std::int64_t player_id,
                                                           std::int64_t session_id,
                                                           int energy_refund,
                                                           const std::string& idempotency_key) {
    const auto result = player_repository_.CancelBattleEntry(player_id, session_id, energy_refund, idempotency_key);
    if (!result.success) {
        return {false, MapMutationError(result.error), result.error_message};
    }

    RefreshPlayerCacheBestEffort(player_id);
    return {true, common::error::ErrorCode::kOk, ""};
}

// 状态推进：`ApplyRewardGrant` 执行写链或补偿并收敛状态变化。
ApplyRewardGrantResponse PlayerService::ApplyRewardGrant(std::int64_t player_id,
                                                         std::int64_t grant_id,
                                                         std::int64_t session_id,
                                                         const std::vector<common::model::Reward>& rewards,
                                                         const std::string& idempotency_key) {
    const auto result = player_repository_.ApplyRewardGrant(player_id, grant_id, session_id, rewards, idempotency_key);
    if (!result.success) {
        return {false, MapMutationError(result.error), result.error_message, {}};
    }

    RefreshPlayerCacheBestEffort(player_id);
    return {true, common::error::ErrorCode::kOk, "", result.applied_currencies};
}

std::optional<common::model::PlayerState> PlayerService::LoadCachedPlayer(std::int64_t player_id) const {
    return player_cache_repository_.FindByPlayerId(player_id);
}

std::optional<common::model::PlayerState> PlayerService::LoadPlayerFromStorage(std::int64_t player_id) const {
    return player_repository_.LoadPlayerState(player_id);
}

LoadPlayerResponse PlayerService::BuildLoadSuccess(const common::model::PlayerState& player_state,
                                                   bool loaded_from_cache) const {
    return BuildLoadPlayerSuccess(player_state, loaded_from_cache);
}

void PlayerService::RefreshPlayerCacheBestEffort(std::int64_t player_id) const {
    if (const auto player_state = LoadPlayerFromStorage(player_id); player_state.has_value()) {
        if (player_cache_repository_.Save(*player_state)) {
            return;
        }
        if (player_cache_repository_.Invalidate(player_id)) {
            return;
        }
    } else if (player_cache_repository_.Invalidate(player_id)) {
        return;
    }

    common::log::Logger::Instance().Log(
        common::log::LogLevel::kWarn,
        "player cache refresh failed after mutation for player_id=" + std::to_string(player_id));
}

}  // namespace game_server::player

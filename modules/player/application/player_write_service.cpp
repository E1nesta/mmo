// 代码规范落地：业务编排层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/player/application/player_write_service.h"

#include "runtime/foundation/log/logger.h"

namespace game_server::player {

namespace {

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

BattleEntrySnapshotResponse BuildBattleEntrySnapshotSuccess(std::int64_t player_id,
                                                            int level,
                                                            int energy,
                                                            std::vector<common::model::PlayerRoleSummary> role_summaries) {
    return {true, common::error::ErrorCode::kOk, "", true, player_id, level, energy, std::move(role_summaries)};
}

BattleEntrySnapshotResponse BuildBattleEntrySnapshotMissing() {
    return {true, common::error::ErrorCode::kOk, "", false, 0, 0, 0, {}};
}

}  // namespace

PlayerWriteService::PlayerWriteService(PlayerRepository& player_repository,
                                       PlayerCacheRepository& player_cache_repository)
    : player_repository_(player_repository),
      player_cache_repository_(player_cache_repository) {}

// 状态读取：`GetBattleEntrySnapshot` 负责加载上下文并返回稳定结果。
BattleEntrySnapshotResponse PlayerWriteService::GetBattleEntrySnapshot(std::int64_t player_id) {
    // 读取阶段：从正式存储读取入战快照，作为副本链前置输入。
    const auto result = player_repository_.GetBattleEntrySnapshot(player_id);
    if (!result.success) {
        return {false, MapMutationError(result.error), result.error_message, false, 0, 0, 0, {}};
    }
    if (!result.found) {
        return BuildBattleEntrySnapshotMissing();
    }
    return BuildBattleEntrySnapshotSuccess(player_id, result.level, result.energy, std::move(result.role_summaries));
}

// 状态推进：`PrepareBattleEntry` 执行写链或补偿并收敛状态变化。
PrepareBattleEntryResponse PlayerWriteService::PrepareBattleEntry(std::int64_t player_id,
                                                                  std::int64_t session_id,
                                                                  int energy_cost,
                                                                  const std::string& idempotency_key) {
    // 写入阶段：正式链路预扣体力，并绑定本次幂等键。
    const auto result = player_repository_.PrepareBattleEntry(player_id, session_id, energy_cost, idempotency_key);
    if (!result.success) {
        return {false, MapMutationError(result.error), result.error_message, 0};
    }

    // 收敛阶段：写链成功后失效查询缓存，避免读到旧快照。
    InvalidatePlayerCacheBestEffort(player_id);
    return {true, common::error::ErrorCode::kOk, "", result.remain_energy};
}

CancelBattleEntryResponse PlayerWriteService::CancelBattleEntry(std::int64_t player_id,
                                                                std::int64_t session_id,
                                                                int energy_refund,
                                                                const std::string& idempotency_key) {
    // 补偿阶段：回滚入战预扣，恢复正式状态。
    const auto result = player_repository_.CancelBattleEntry(player_id, session_id, energy_refund, idempotency_key);
    if (!result.success) {
        return {false, MapMutationError(result.error), result.error_message};
    }

    // 收敛阶段：补偿成功后同样失效查询缓存。
    InvalidatePlayerCacheBestEffort(player_id);
    return {true, common::error::ErrorCode::kOk, ""};
}

// 状态推进：`ApplyRewardGrant` 执行写链或补偿并收敛状态变化。
ApplyRewardGrantResponse PlayerWriteService::ApplyRewardGrant(std::int64_t player_id,
                                                              std::int64_t grant_id,
                                                              std::int64_t session_id,
                                                              const std::vector<common::model::Reward>& rewards,
                                                              const std::string& idempotency_key) {
    // 写入阶段：正式发奖并记录幂等键，避免重复落账。
    const auto result = player_repository_.ApplyRewardGrant(player_id, grant_id, session_id, rewards, idempotency_key);
    if (!result.success) {
        return {false, MapMutationError(result.error), result.error_message, {}};
    }

    // 收敛阶段：发奖完成后失效查询缓存，确保前台读到新资产。
    InvalidatePlayerCacheBestEffort(player_id);
    return {true, common::error::ErrorCode::kOk, "", result.applied_currencies};
}

// 状态推进：`InvalidatePlayerCacheBestEffort` 执行写链或补偿并收敛状态变化。
void PlayerWriteService::InvalidatePlayerCacheBestEffort(std::int64_t player_id) const {
    // 尽力阶段：缓存失效失败不阻断主写链，但需要明确记录告警。
    if (player_cache_repository_.Invalidate(player_id)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kInfo,
            "player cache invalidated after mutation: player_id=" + std::to_string(player_id));
        return;
    }

    common::log::Logger::Instance().Log(
        common::log::LogLevel::kWarn,
        "player cache invalidate failed after mutation for player_id=" + std::to_string(player_id));
}

}  // namespace game_server::player

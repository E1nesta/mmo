// 代码规范落地：基础设施层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/player/infrastructure/in_memory_player_repository.h"

#include <algorithm>

namespace game_server::player {

InMemoryPlayerRepository InMemoryPlayerRepository::FromConfig(const common::config::SimpleConfig& config) {
    common::model::PlayerState player_state;
    player_state.profile.player_id = config.GetInt("demo.player_id", 20001);
    player_state.profile.account_id = config.GetInt("demo.account_id", 10001);
    player_state.profile.server_id = config.GetInt("demo.server_id", 1);
    player_state.profile.player_name = config.GetString("demo.player_name", "hero_demo");
    player_state.profile.nickname = config.GetString("demo.nickname", player_state.profile.player_name);
    player_state.profile.level = config.GetInt("demo.level", 10);
    player_state.profile.stamina = config.GetInt("demo.stamina", 120);
    player_state.profile.gold = config.GetInt("demo.gold", 1000);
    player_state.profile.diamond = config.GetInt("demo.diamond", 100);
    player_state.profile.main_stage_id = config.GetInt("demo.main_stage_id", 1001);
    player_state.profile.main_chapter_id =
        config.GetInt("demo.main_chapter_id", player_state.profile.main_stage_id > 0 ? player_state.profile.main_stage_id / 1000 : 0);
    player_state.profile.fight_power = config.GetInt("demo.fight_power", 1200);
    player_state.currencies.push_back({"diamond", player_state.profile.diamond});
    player_state.currencies.push_back({"gold", player_state.profile.gold});
    player_state.role_summaries.push_back({1001, player_state.profile.level, 1});
    player_state.role_summaries.push_back({1002, player_state.profile.level, 1});
    player_state.role_summaries.push_back({1003, player_state.profile.level, 1});
    if (player_state.profile.main_stage_id > 0) {
        player_state.stage_progress.push_back({player_state.profile.main_stage_id, 3, true});
    }
    return InMemoryPlayerRepository(player_state);
}

// 状态读取：`LoadPlayerState` 负责加载上下文并返回稳定结果。
std::optional<common::model::PlayerState> InMemoryPlayerRepository::LoadPlayerState(std::int64_t player_id) const {
    const auto iter = players_.find(player_id);
    if (iter == players_.end()) {
        return std::nullopt;
    }
    return iter->second;
}

// 状态读取：`GetBattleEntrySnapshot` 负责加载上下文并返回稳定结果。
BattleEntrySnapshotResult InMemoryPlayerRepository::GetBattleEntrySnapshot(std::int64_t player_id) const {
    const auto player = players_.find(player_id);
    if (player == players_.end()) {
        return {true, false, 0, 0, {}, PlayerMutationError::kNone, ""};
    }
    return {true,
            true,
            player->second.profile.level,
            player->second.profile.stamina,
            player->second.role_summaries,
            PlayerMutationError::kNone,
            ""};
}

// 状态推进：`PrepareBattleEntry` 执行写链或补偿并收敛状态变化。
PrepareBattleEntryResult InMemoryPlayerRepository::PrepareBattleEntry(std::int64_t player_id,
                                                                      std::int64_t session_id,
                                                                      int energy_cost,
                                                                      const std::string& idempotency_key) {
    (void)session_id;
    if (const auto operation = enter_operations_.find(idempotency_key); operation != enter_operations_.end()) {
        if (operation->second.player_id != player_id || operation->second.energy_cost != energy_cost) {
            return {false, 0, PlayerMutationError::kBattleMismatch, "battle entry request mismatch"};
        }
        return operation->second.result;
    }

    auto player = players_.find(player_id);
    if (player == players_.end()) {
        return {false, 0, PlayerMutationError::kPlayerNotFound, "player not found"};
    }
    if (player->second.profile.stamina < energy_cost) {
        return {false, player->second.profile.stamina, PlayerMutationError::kStaminaNotEnough, "stamina not enough"};
    }

    player->second.profile.stamina -= energy_cost;
    const PrepareBattleEntryResult result{true, player->second.profile.stamina, PlayerMutationError::kNone, ""};
    enter_operations_.emplace(idempotency_key, EnterOperationRecord{player_id, energy_cost, result});
    return result;
}

CancelBattleEntryResult InMemoryPlayerRepository::CancelBattleEntry(std::int64_t player_id,
                                                                    std::int64_t session_id,
                                                                    int energy_refund,
                                                                    const std::string& idempotency_key) {
    (void)session_id;
    if (enter_operations_.find(idempotency_key) != enter_operations_.end()) {
        return {true, PlayerMutationError::kNone, ""};
    }
    auto player = players_.find(player_id);
    if (player == players_.end()) {
        return {false, PlayerMutationError::kPlayerNotFound, "player not found"};
    }
    player->second.profile.stamina += energy_refund;
    return {true, PlayerMutationError::kNone, ""};
}

// 状态推进：`ApplyRewardGrant` 执行写链或补偿并收敛状态变化。
ApplyRewardGrantResult InMemoryPlayerRepository::ApplyRewardGrant(std::int64_t player_id,
                                                                  std::int64_t grant_id,
                                                                  std::int64_t session_id,
                                                                  const std::vector<common::model::Reward>& rewards,
                                                                  const std::string& idempotency_key) {
    (void)session_id;
    if (const auto operation = reward_operations_.find(idempotency_key); operation != reward_operations_.end()) {
        if (operation->second.player_id != player_id || operation->second.grant_id != grant_id) {
            return {false, {}, PlayerMutationError::kBattleMismatch, "reward grant request mismatch"};
        }
        return operation->second.result;
    }

    auto player = players_.find(player_id);
    if (player == players_.end()) {
        return {false, {}, PlayerMutationError::kPlayerNotFound, "player not found"};
    }

    ApplyRewardGrantResult result;
    result.success = true;
    for (const auto& reward : rewards) {
        if (reward.reward_type == "gold") {
            player->second.profile.gold += reward.amount;
        } else if (reward.reward_type == "diamond") {
            player->second.profile.diamond += reward.amount;
        }
        result.applied_currencies.push_back({reward.reward_type, reward.amount});
    }
    reward_operations_.emplace(idempotency_key, RewardGrantOperationRecord{player_id, grant_id, result});
    return result;
}

InMemoryPlayerRepository::InMemoryPlayerRepository(common::model::PlayerState player_state) {
    players_.emplace(player_state.profile.player_id, std::move(player_state));
}

}  // namespace game_server::player

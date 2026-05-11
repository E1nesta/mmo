#include "modules/player/player_service.h"

#include "modules/player/player_repository.h"

#include <utility>

namespace modules::player {

PlayerService::PlayerService(std::shared_ptr<PlayerRepository> repository)
    : repository_(std::move(repository)) {}

ApplyRewardResult PlayerService::apply_reward(
    PlayerState& state,
    std::int64_t player_id,
    const std::string& idempotency_key,
    const std::vector<Reward>& rewards) {
    if (repository_ != nullptr) {
        ApplyRewardResult result;
        std::string error_message;
        if (repository_->apply_reward_once(
                player_id, idempotency_key, rewards, &result, &error_message)) {
            return result;
        }
        result.success = false;
        result.applied = false;
        result.error_code = 500;
        result.error_message =
            error_message.empty() ? "failed to apply reward" : error_message;
        return result;
    }

    auto& profile = profile_for(state, player_id);
    if (state.reward_ledger.count(idempotency_key) > 0) {
        ApplyRewardResult result;
        result.applied = false;
        result.gold = profile.gold;
        result.exp = profile.exp;
        return result;
    }

    for (const auto& reward : rewards) {
        if (reward.type == "gold") {
            profile.gold += reward.amount;
        } else if (reward.type == "exp") {
            profile.exp += reward.amount;
        }
    }

    state.reward_ledger.insert(idempotency_key);
    ApplyRewardResult result;
    result.applied = true;
    result.gold = profile.gold;
    result.exp = profile.exp;
    return result;
}

PlayerProfile& PlayerService::profile_for(
    PlayerState& state,
    std::int64_t player_id) {
    if (!state.initialized || state.profile.player_id != player_id) {
        state.profile = PlayerProfile{player_id, 0, 0};
        state.initialized = true;
    }
    return state.profile;
}

}  // namespace modules::player

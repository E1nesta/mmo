#include "modules/player/player_service.h"

namespace mmo::modules::player {

ApplyRewardResult PlayerService::apply_reward(
    std::int64_t player_id,
    const std::string& idempotency_key,
    const std::vector<Reward>& rewards) {
    auto& profile = profile_for(player_id);
    if (reward_ledger_.count(idempotency_key) > 0) {
        return ApplyRewardResult{false, profile.gold, profile.exp};
    }

    for (const auto& reward : rewards) {
        if (reward.type == "gold") {
            profile.gold += reward.amount;
        } else if (reward.type == "exp") {
            profile.exp += reward.amount;
        }
    }

    reward_ledger_.insert(idempotency_key);
    return ApplyRewardResult{true, profile.gold, profile.exp};
}

PlayerProfile& PlayerService::profile_for(std::int64_t player_id) {
    for (auto& profile : profiles_) {
        if (profile.player_id == player_id) {
            return profile;
        }
    }
    profiles_.push_back(PlayerProfile{player_id, 0, 0});
    return profiles_.back();
}

}  // namespace mmo::modules::player

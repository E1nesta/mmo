#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "modules/player/player_types.h"

namespace modules::player {

class PlayerRepository;

struct PlayerState {
    PlayerProfile profile;
    std::set<std::string> reward_ledger;
    bool initialized{};
};

class PlayerService {
public:
    PlayerService() = default;
    explicit PlayerService(std::shared_ptr<PlayerRepository> repository);

    ApplyRewardResult apply_reward(
        PlayerState& state,
        std::int64_t player_id,
        const std::string& idempotency_key,
        const std::vector<Reward>& rewards);

private:
    static PlayerProfile& profile_for(PlayerState& state, std::int64_t player_id);

    std::shared_ptr<PlayerRepository> repository_;
};

}  // namespace modules::player

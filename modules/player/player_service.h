#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "modules/player/player_types.h"

namespace mmo::modules::player {

class PlayerRepository;

class PlayerService {
public:
    PlayerService() = default;
    explicit PlayerService(std::shared_ptr<PlayerRepository> repository);

    ApplyRewardResult apply_reward(
        std::int64_t player_id,
        const std::string& idempotency_key,
        const std::vector<Reward>& rewards);

private:
    PlayerProfile& profile_for(std::int64_t player_id);

    std::shared_ptr<PlayerRepository> repository_;
    std::vector<PlayerProfile> profiles_;
    std::set<std::string> reward_ledger_;
};

}  // namespace mmo::modules::player

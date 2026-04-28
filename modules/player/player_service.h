#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace mmo::modules::player {

class PlayerRepository;

struct Reward {
    std::string type;
    std::int64_t amount{};
};

struct PlayerProfile {
    std::int64_t player_id{};
    std::int64_t gold{};
    std::int64_t exp{};
};

struct ApplyRewardResult {
    bool applied{};
    std::int64_t gold{};
    std::int64_t exp{};
};

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

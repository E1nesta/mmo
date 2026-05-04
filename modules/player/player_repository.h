#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "modules/player/player_types.h"

namespace modules::player {

class PlayerRepository {
public:
    virtual ~PlayerRepository() = default;

    virtual std::optional<PlayerProfile> load_profile(std::int64_t player_id) = 0;
    virtual bool save_profile(const PlayerProfile& profile, std::string* error_message) = 0;
    virtual bool record_reward_ledger(
        const RewardLedgerRecord& record,
        std::string* error_message) = 0;
    virtual bool apply_reward_once(
        std::int64_t player_id,
        const std::string& idempotency_key,
        const std::vector<Reward>& rewards,
        ApplyRewardResult* result,
        std::string* error_message) = 0;
};

}  // namespace modules::player

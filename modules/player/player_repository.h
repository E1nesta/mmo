#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "modules/player/player_service.h"

namespace mmo::modules::player {

struct RewardLedgerRecord {
    std::int64_t player_id{};
    std::string idempotency_key;
    std::string request_id;
    std::int64_t gold{};
    std::int64_t exp{};
};

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

}  // namespace mmo::modules::player

#pragma once

#include <cstdint>
#include <string>

namespace modules::player {

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
    bool success{true};
    bool applied{};
    int error_code{};
    std::string error_message;
    std::int64_t gold{};
    std::int64_t exp{};
};

struct RewardLedgerRecord {
    std::int64_t player_id{};
    std::string idempotency_key;
    std::string request_id;
    std::int64_t gold{};
    std::int64_t exp{};
};

}  // namespace modules::player

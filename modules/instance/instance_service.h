#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "modules/player/player_types.h"

namespace modules::instance {

struct InstanceContext {
    std::int64_t instance_id{};
    std::int64_t player_id{};
    int dungeon_id{};
    std::int64_t boss_entity_id{};
};

struct InstanceState {
    InstanceContext context;
    std::set<std::string> settled_keys;
};

struct SettleResult {
    std::string reward_grant_id;
    std::vector<modules::player::Reward> rewards;
    bool duplicate{};
};

class InstanceService {
public:
    InstanceContext enter_instance(
        std::int64_t instance_id,
        std::int64_t player_id,
        int dungeon_id) const;
    SettleResult settle_instance(
        InstanceState& state,
        std::int64_t player_id,
        const std::string& idempotency_key,
        bool win) const;
};

}  // namespace modules::instance

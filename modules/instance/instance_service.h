#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "modules/player/player_types.h"

namespace mmo::modules::instance {

struct InstanceContext {
    std::int64_t instance_id{};
    std::int64_t player_id{};
    int dungeon_id{};
    std::int64_t boss_entity_id{};
};

struct SettleResult {
    std::string reward_grant_id;
    std::vector<mmo::modules::player::Reward> rewards;
    bool duplicate{};
};

class InstanceService {
public:
    InstanceContext enter_instance(std::int64_t player_id, int dungeon_id);
    SettleResult settle_instance(
        std::int64_t player_id,
        std::int64_t instance_id,
        const std::string& idempotency_key,
        bool win);

private:
    std::int64_t next_instance_id_{500000};
    std::unordered_map<std::int64_t, InstanceContext> instances_;
    std::set<std::string> settled_keys_;
};

}  // namespace mmo::modules::instance

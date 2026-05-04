#include "modules/instance/instance_service.h"

namespace modules::instance {

InstanceContext InstanceService::enter_instance(std::int64_t player_id, int dungeon_id) {
    InstanceContext context;
    context.instance_id = next_instance_id_++;
    context.player_id = player_id;
    context.dungeon_id = dungeon_id;
    context.boss_entity_id = context.instance_id * 10 + 1;
    instances_[context.instance_id] = context;
    return context;
}

SettleResult InstanceService::settle_instance(
    std::int64_t player_id,
    std::int64_t instance_id,
    const std::string& idempotency_key,
    bool win) {
    SettleResult result;
    result.reward_grant_id = "grant-" + std::to_string(instance_id);
    if (settled_keys_.count(idempotency_key) > 0) {
        result.duplicate = true;
        return result;
    }

    settled_keys_.insert(idempotency_key);
    result.duplicate = false;
    if (win) {
        result.rewards.push_back(modules::player::Reward{"gold", 100});
        result.rewards.push_back(modules::player::Reward{"exp", 50});
    }
    instances_[instance_id].player_id = player_id;
    return result;
}

}  // namespace modules::instance

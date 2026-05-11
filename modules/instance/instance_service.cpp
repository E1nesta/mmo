#include "modules/instance/instance_service.h"

namespace modules::instance {

InstanceContext InstanceService::enter_instance(
    std::int64_t instance_id,
    std::int64_t player_id,
    int dungeon_id) const {
    InstanceContext context;
    context.instance_id = instance_id;
    context.player_id = player_id;
    context.dungeon_id = dungeon_id;
    context.boss_entity_id = context.instance_id * 10 + 1;
    return context;
}

SettleResult InstanceService::settle_instance(
    InstanceState& state,
    std::int64_t player_id,
    const std::string& idempotency_key,
    bool win) const {
    SettleResult result;
    result.reward_grant_id =
        "grant-" + std::to_string(state.context.instance_id);
    if (state.settled_keys.count(idempotency_key) > 0) {
        result.duplicate = true;
        return result;
    }

    state.settled_keys.insert(idempotency_key);
    result.duplicate = false;
    if (win) {
        result.rewards.push_back(modules::player::Reward{"gold", 100});
        result.rewards.push_back(modules::player::Reward{"exp", 50});
    }
    state.context.player_id = player_id;
    return result;
}

}  // namespace modules::instance

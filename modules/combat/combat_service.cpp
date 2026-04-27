#include "modules/combat/combat_service.h"

#include <algorithm>

namespace mmo::modules::combat {

CombatResult CombatService::cast_single_skill(
    const AttributeSet& attacker,
    const AttributeSet& target,
    int skill_power) const {
    const std::int64_t raw_damage = attacker.attack + skill_power - target.defense;
    CombatResult result;
    result.damage = std::max<std::int64_t>(1, raw_damage);
    result.target_dead = result.damage >= target.hp;
    return result;
}

}  // namespace mmo::modules::combat

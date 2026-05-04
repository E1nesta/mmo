#pragma once

#include <cstdint>

namespace modules::combat {

struct AttributeSet {
    std::int64_t hp{};
    std::int64_t attack{};
    std::int64_t defense{};
};

struct CombatResult {
    std::int64_t damage{};
    bool target_dead{};
};

class CombatService {
public:
    CombatResult cast_single_skill(
        const AttributeSet& attacker,
        const AttributeSet& target,
        int skill_power) const;
};

}  // namespace modules::combat

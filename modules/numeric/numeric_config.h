#pragma once

#include "modules/combat/combat_service.h"

namespace mmo::modules::numeric {

class NumericConfig {
public:
    mmo::modules::combat::AttributeSet default_player_attributes() const;
    mmo::modules::combat::AttributeSet default_boss_attributes() const;
    int default_skill_power() const;
};

}  // namespace mmo::modules::numeric

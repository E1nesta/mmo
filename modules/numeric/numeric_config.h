#pragma once

#include "modules/combat/combat_service.h"

namespace modules::numeric {

class NumericConfig {
public:
    modules::combat::AttributeSet default_player_attributes() const;
    modules::combat::AttributeSet default_boss_attributes() const;
    int default_skill_power() const;
};

}  // namespace modules::numeric

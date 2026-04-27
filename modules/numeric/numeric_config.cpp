#include "modules/numeric/numeric_config.h"

namespace mmo::modules::numeric {

mmo::modules::combat::AttributeSet NumericConfig::default_player_attributes() const {
    return mmo::modules::combat::AttributeSet{1000, 120, 20};
}

mmo::modules::combat::AttributeSet NumericConfig::default_boss_attributes() const {
    return mmo::modules::combat::AttributeSet{3000, 180, 35};
}

int NumericConfig::default_skill_power() const {
    return 50;
}

}  // namespace mmo::modules::numeric

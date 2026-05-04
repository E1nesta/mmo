#include "modules/movement/movement_service.h"

#include <cmath>

namespace modules::movement {

bool MovementService::validate(const MoveCommand& command) const {
    const float dx = command.to.x - command.from.x;
    const float dy = command.to.y - command.from.y;
    const float dz = command.to.z - command.from.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    return distance <= command.max_distance;
}

}  // namespace modules::movement

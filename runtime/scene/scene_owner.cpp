#include "runtime/scene/scene_owner.h"

#include <stdexcept>
#include <utility>

namespace runtime::scene {

SceneOwner::SceneOwner(SceneOwnerOptions options) : options_(std::move(options)) {
    if (options_.scene_id <= 0) {
        throw std::invalid_argument("scene_id must be positive");
    }
    if (options_.tick_interval_millis == 0) {
        throw std::invalid_argument("scene tick interval must be positive");
    }
}

SceneId SceneOwner::scene_id() const {
    return options_.scene_id;
}

const std::string& SceneOwner::world_id() const {
    return options_.world_id;
}

std::uint32_t SceneOwner::tick_interval_millis() const {
    return options_.tick_interval_millis;
}

SceneOwnerState SceneOwner::state() const {
    return state_;
}

void SceneOwner::start() {
    state_ = SceneOwnerState::kRunning;
}

void SceneOwner::drain() {
    state_ = SceneOwnerState::kDraining;
}

void SceneOwner::stop() {
    state_ = SceneOwnerState::kStopped;
}

}  // namespace runtime::scene

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "runtime/scene/scene_owner.h"

namespace runtime::scene {

struct SceneDelta {
    std::uint64_t entity_id{};
    std::uint32_t message_id{};
    std::string payload;
};

struct SceneSnapshot {
    SceneId scene_id{};
    std::uint64_t tick_id{};
    std::vector<SceneDelta> deltas;
};

}  // namespace runtime::scene

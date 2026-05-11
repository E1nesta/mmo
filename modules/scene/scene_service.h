#pragma once

#include <cstdint>
#include <unordered_map>

namespace modules::scene {

struct Transform {
    float x{};
    float y{};
    float z{};
};

struct SceneEntity {
    std::int64_t entity_id{};
    std::int64_t player_id{};
    std::int64_t scene_id{};
    Transform transform;
};

struct SceneState {
    std::int64_t next_entity_id{1000000};
    std::unordered_map<std::int64_t, SceneEntity> entities;
};

class SceneService {
public:
    SceneEntity enter_scene(
        SceneState& state,
        std::int64_t player_id,
        std::int64_t scene_id) const;
};

}  // namespace modules::scene

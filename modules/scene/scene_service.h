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

class SceneService {
public:
    SceneEntity enter_scene(std::int64_t player_id, std::int64_t scene_id);

private:
    std::int64_t next_entity_id_{1000000};
    std::unordered_map<std::int64_t, SceneEntity> entities_;
};

}  // namespace modules::scene

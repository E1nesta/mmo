#include "modules/scene/scene_service.h"

namespace modules::scene {

SceneEntity SceneService::enter_scene(
    SceneState& state,
    std::int64_t player_id,
    std::int64_t scene_id) const {
    SceneEntity entity;
    entity.entity_id = state.next_entity_id++;
    entity.player_id = player_id;
    entity.scene_id = scene_id;
    entity.transform = Transform{0.0F, 0.0F, 0.0F};
    state.entities[entity.entity_id] = entity;
    return entity;
}

}  // namespace modules::scene

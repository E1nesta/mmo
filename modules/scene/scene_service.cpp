#include "modules/scene/scene_service.h"

namespace modules::scene {

SceneEntity SceneService::enter_scene(std::int64_t player_id, std::int64_t scene_id) {
    SceneEntity entity;
    entity.entity_id = next_entity_id_++;
    entity.player_id = player_id;
    entity.scene_id = scene_id;
    entity.transform = Transform{0.0F, 0.0F, 0.0F};
    entities_[entity.entity_id] = entity;
    return entity;
}

}  // namespace modules::scene

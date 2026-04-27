#include "modules/world/world_service.h"

namespace mmo::modules::world {

SceneRoute WorldService::enter_world(
    std::int64_t player_id,
    int preferred_map_id,
    int preferred_line_id) {
    SceneRoute route;
    route.map_id = preferred_map_id > 0 ? preferred_map_id : 1001;
    route.line_id = preferred_line_id > 0 ? preferred_line_id : 1;
    route.scene_id = static_cast<std::int64_t>(route.map_id) * 100 + route.line_id;

    online_players_[player_id] = OnlinePlayer{player_id, route};
    return route;
}

}  // namespace mmo::modules::world

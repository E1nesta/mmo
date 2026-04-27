#pragma once

#include <cstdint>
#include <unordered_map>

namespace mmo::modules::world {

struct SceneRoute {
    int map_id{};
    int line_id{};
    std::int64_t scene_id{};
};

struct OnlinePlayer {
    std::int64_t player_id{};
    SceneRoute route;
};

class WorldService {
public:
    SceneRoute enter_world(std::int64_t player_id, int preferred_map_id, int preferred_line_id);

private:
    std::unordered_map<std::int64_t, OnlinePlayer> online_players_;
};

}  // namespace mmo::modules::world

#pragma once

#include <cstdint>
#include <unordered_map>

namespace modules::world {

struct SceneRoute {
    int map_id{};
    int line_id{};
    std::int64_t scene_id{};
};

struct OnlinePlayer {
    std::int64_t player_id{};
    SceneRoute route;
};

struct WorldState {
    std::unordered_map<std::int64_t, OnlinePlayer> online_players;
};

class WorldService {
public:
    SceneRoute enter_world(
        WorldState& state,
        std::int64_t player_id,
        int preferred_map_id,
        int preferred_line_id) const;
};

}  // namespace modules::world

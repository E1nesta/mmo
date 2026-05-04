#pragma once

#include <cstdint>

namespace modules::social {

struct SocialBoundary {
    std::int64_t player_id{};
    bool friend_boundary_available{};
    bool chat_boundary_available{};
    bool team_boundary_available{};
};

class SocialBoundaryService {
public:
    SocialBoundary boundary_for(std::int64_t player_id) const;
};

}  // namespace modules::social

#include "modules/social/social_boundary.h"

namespace modules::social {

SocialBoundary SocialBoundaryService::boundary_for(std::int64_t player_id) const {
    return SocialBoundary{player_id, true, true, true};
}

}  // namespace modules::social

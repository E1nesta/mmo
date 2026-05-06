#include "runtime/scene/scene_tick.h"

namespace runtime::scene {

SceneTick SceneTickClock::next(std::uint64_t now_millis) {
    SceneTick tick;
    tick.tick_id = next_tick_id_++;
    tick.started_at_millis = now_millis;
    return tick;
}

}  // namespace runtime::scene

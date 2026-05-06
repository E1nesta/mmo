#pragma once

#include <cstdint>

namespace runtime::scene {

struct SceneTick {
    std::uint64_t tick_id{};
    std::uint64_t started_at_millis{};
    std::uint64_t duration_micros{};
    std::uint32_t input_count{};
    std::uint32_t snapshot_count{};
};

class SceneTickClock {
public:
    SceneTick next(std::uint64_t now_millis);

private:
    std::uint64_t next_tick_id_{1};
};

}  // namespace runtime::scene

#pragma once

#include <cstdint>
#include <string>

namespace runtime::scene {

using SceneId = std::int64_t;

enum class SceneOwnerState {
    kStopped = 0,
    kRunning = 1,
    kDraining = 2,
};

struct SceneOwnerOptions {
    SceneId scene_id{};
    std::string world_id;
    std::uint32_t tick_interval_millis{50};
};

class SceneOwner {
public:
    explicit SceneOwner(SceneOwnerOptions options);

    SceneId scene_id() const;
    const std::string& world_id() const;
    std::uint32_t tick_interval_millis() const;
    SceneOwnerState state() const;

    void start();
    void drain();
    void stop();

private:
    SceneOwnerOptions options_;
    SceneOwnerState state_{SceneOwnerState::kStopped};
};

}  // namespace runtime::scene

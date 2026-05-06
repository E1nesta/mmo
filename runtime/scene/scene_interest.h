#pragma once

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "runtime/scene/scene_owner.h"

namespace runtime::scene {

class SceneInterest {
public:
    void watch(SceneId scene_id, std::uint64_t observer_id, std::uint64_t target_id);
    void unwatch(SceneId scene_id, std::uint64_t observer_id, std::uint64_t target_id);
    bool interested(
        SceneId scene_id,
        std::uint64_t observer_id,
        std::uint64_t target_id) const;

private:
    using TargetSet = std::unordered_set<std::uint64_t>;
    using ObserverMap = std::unordered_map<std::uint64_t, TargetSet>;

    std::unordered_map<SceneId, ObserverMap> scene_interest_;
};

}  // namespace runtime::scene

#include "runtime/scene/scene_interest.h"

namespace runtime::scene {

void SceneInterest::watch(
    SceneId scene_id,
    std::uint64_t observer_id,
    std::uint64_t target_id) {
    scene_interest_[scene_id][observer_id].insert(target_id);
}

void SceneInterest::unwatch(
    SceneId scene_id,
    std::uint64_t observer_id,
    std::uint64_t target_id) {
    const auto scene_it = scene_interest_.find(scene_id);
    if (scene_it == scene_interest_.end()) {
        return;
    }
    const auto observer_it = scene_it->second.find(observer_id);
    if (observer_it == scene_it->second.end()) {
        return;
    }
    observer_it->second.erase(target_id);
}

bool SceneInterest::interested(
    SceneId scene_id,
    std::uint64_t observer_id,
    std::uint64_t target_id) const {
    const auto scene_it = scene_interest_.find(scene_id);
    if (scene_it == scene_interest_.end()) {
        return false;
    }
    const auto observer_it = scene_it->second.find(observer_id);
    if (observer_it == scene_it->second.end()) {
        return false;
    }
    return observer_it->second.find(target_id) != observer_it->second.end();
}

}  // namespace runtime::scene

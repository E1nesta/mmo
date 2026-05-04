#include "runtime/execution/scene_worker.h"

#include <utility>

namespace runtime::execution {

SceneWorker::SceneWorker(
    std::int64_t scene_id,
    std::shared_ptr<ShardedExecutor> executor)
    : scene_id_(scene_id), executor_(std::move(executor)) {}

std::int64_t SceneWorker::scene_id() const {
    return scene_id_;
}

void SceneWorker::post(Task task) {
    (void)executor_->post(static_cast<std::uint64_t>(scene_id_), std::move(task));
}

}  // namespace runtime::execution

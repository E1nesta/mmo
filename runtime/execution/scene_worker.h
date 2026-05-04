#pragma once

#include <cstdint>
#include <functional>
#include <memory>

#include "runtime/execution/sharded_executor.h"

namespace runtime::execution {

class SceneWorker {
public:
    using Task = ShardedExecutor::Task;

    SceneWorker(std::int64_t scene_id, std::shared_ptr<ShardedExecutor> executor);

    std::int64_t scene_id() const;
    void post(Task task);

private:
    std::int64_t scene_id_{};
    std::shared_ptr<ShardedExecutor> executor_;
};

}  // namespace runtime::execution

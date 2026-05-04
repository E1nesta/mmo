#include "runtime/execution/instance_shard.h"

#include <utility>

namespace runtime::execution {

InstanceShard::InstanceShard(std::shared_ptr<ShardedExecutor> executor)
    : executor_(std::move(executor)) {}

void InstanceShard::post(std::int64_t instance_id, Task task) {
    (void)executor_->post(static_cast<std::uint64_t>(instance_id), std::move(task));
}

}  // namespace runtime::execution

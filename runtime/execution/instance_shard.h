#pragma once

#include <cstdint>
#include <memory>

#include "runtime/execution/sharded_executor.h"

namespace mmo::runtime::execution {

class InstanceShard {
public:
    using Task = ShardedExecutor::Task;

    explicit InstanceShard(std::shared_ptr<ShardedExecutor> executor);

    void post(std::int64_t instance_id, Task task);

private:
    std::shared_ptr<ShardedExecutor> executor_;
};

}  // namespace mmo::runtime::execution

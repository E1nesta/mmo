#pragma once

#include <cstddef>
#include <functional>

#include "runtime/entity/entity_mailbox.h"
#include "runtime/entity/entity_router.h"
#include "runtime/scheduler/sharded_executor.h"

namespace runtime::entity {

struct ScheduledEntityDrain {
    runtime::scheduler::PostStatus status{
        runtime::scheduler::PostStatus::kStopped};

    bool accepted() const;
};

class EntityExecutor {
public:
    using Handler = std::function<void(const EntityMessage&)>;

    explicit EntityExecutor(EntityRouter& router);

    ScheduledEntityDrain submit(
        runtime::scheduler::ShardedExecutor& scheduler,
        EntityMessage message,
        Handler handler);
    std::size_t drain_tasks(EntityId entity_id, std::size_t max_messages);

private:
    ScheduledEntityDrain schedule_drain(
        runtime::scheduler::ShardedExecutor& scheduler,
        EntityId entity_id);

    EntityRouter& router_;
};

}  // namespace runtime::entity

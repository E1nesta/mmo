#include "runtime/entity/entity_executor.h"

#include <utility>

namespace runtime::entity {
namespace {

inline constexpr std::size_t kDefaultDrainBatchSize = 32;

class EntityWriterGuard {
public:
    explicit EntityWriterGuard(const std::shared_ptr<EntityMailbox>& mailbox)
        : mailbox_(mailbox), acquired_(mailbox_ != nullptr &&
                                       mailbox_->try_acquire_writer()) {}
    ~EntityWriterGuard() {
        if (acquired_) {
            mailbox_->release_writer();
        }
    }

    bool acquired() const { return acquired_; }

private:
    std::shared_ptr<EntityMailbox> mailbox_;
    bool acquired_{};
};

}  // namespace

bool ScheduledEntityDrain::accepted() const {
    return status == runtime::scheduler::PostStatus::kAccepted;
}

EntityExecutor::EntityExecutor(EntityRouter& router)
    : router_(router) {}

ScheduledEntityDrain EntityExecutor::submit(
    runtime::scheduler::ShardedExecutor& scheduler,
    EntityMessage message,
    Handler handler) {
    ScheduledEntityDrain result;
    if (!is_valid_entity_id(message.entity_id) || !handler) {
        result.status = runtime::scheduler::PostStatus::kStopped;
        return result;
    }

    const auto entity_id = message.entity_id;
    const auto message_id = message.message_id;
    const auto route_key = message.route_key;
    const auto request_id = message.request_id;
    auto mailbox = router_.find_or_bind(entity_id);
    if (mailbox == nullptr ||
        !mailbox->try_push(EntityTask{std::move(message), std::move(handler)})) {
        result.status = runtime::scheduler::PostStatus::kQueueFull;
        return result;
    }

    result = schedule_drain(scheduler, entity_id);
    if (!result.accepted()) {
        mailbox->try_remove(entity_id, message_id, route_key, request_id);
    }
    return result;
}

ScheduledEntityDrain EntityExecutor::schedule_drain(
    runtime::scheduler::ShardedExecutor& scheduler,
    EntityId entity_id) {
    ScheduledEntityDrain result;
    auto mailbox = router_.find(entity_id);
    if (mailbox == nullptr) {
        result.status = runtime::scheduler::PostStatus::kStopped;
        return result;
    }
    if (!mailbox->try_schedule_drain()) {
        result.status = runtime::scheduler::PostStatus::kAccepted;
        return result;
    }

    const auto post_result = scheduler.post(
        entity_id.key,
        [this, &scheduler, entity_id]() {
            drain_tasks(entity_id, kDefaultDrainBatchSize);
            auto mailbox = router_.find(entity_id);
            if (mailbox != nullptr && mailbox->has_pending_tasks()) {
                schedule_drain(scheduler, entity_id);
            }
        });
    result.status = post_result.status;
    if (!result.accepted()) {
        mailbox->clear_scheduled_drain();
    }
    return result;
}

std::size_t EntityExecutor::drain_tasks(
    EntityId entity_id,
    std::size_t max_messages) {
    auto mailbox = router_.find(entity_id);
    if (mailbox == nullptr) {
        return 0;
    }
    EntityWriterGuard writer(mailbox);
    if (!writer.acquired()) {
        return 0;
    }

    std::size_t handled = 0;
    while (handled < max_messages) {
        auto task = mailbox->pop_task();
        if (!task.has_value()) {
            break;
        }
        if (task->handler) {
            task->handler(task->message);
        }
        ++handled;
    }
    return handled;
}

}  // namespace runtime::entity

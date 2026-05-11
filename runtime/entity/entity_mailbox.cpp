#include "runtime/entity/entity_mailbox.h"

#include <utility>

namespace runtime::entity {

EntityMailbox::EntityMailbox(EntityMailboxOptions options)
    : options_(options) {
    if (options_.max_depth == 0) {
        options_.max_depth = 1;
    }
}

bool EntityMailbox::try_push(EntityTask task) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.size() >= options_.max_depth) {
        ++dropped_count_;
        return false;
    }
    queue_.push_back(std::move(task));
    return true;
}

std::optional<EntityTask> EntityMailbox::pop_task() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }
    auto task = std::move(queue_.front());
    queue_.pop_front();
    return task;
}

bool EntityMailbox::try_remove(
    EntityId entity_id,
    std::uint32_t message_id,
    std::uint64_t route_key,
    std::uint64_t request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = queue_.begin(); it != queue_.end(); ++it) {
        const auto& message = it->message;
        if (message.entity_id == entity_id &&
            message.message_id == message_id &&
            message.route_key == route_key &&
            message.request_id == request_id) {
            queue_.erase(it);
            return true;
        }
    }
    return false;
}

bool EntityMailbox::try_schedule_drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (writer_active_ || drain_scheduled_) {
        return false;
    }
    drain_scheduled_ = true;
    return true;
}

void EntityMailbox::clear_scheduled_drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    drain_scheduled_ = false;
}

bool EntityMailbox::try_acquire_writer() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (writer_active_) {
        return false;
    }
    drain_scheduled_ = false;
    writer_active_ = true;
    return true;
}

void EntityMailbox::release_writer() {
    std::lock_guard<std::mutex> lock(mutex_);
    writer_active_ = false;
}

std::size_t EntityMailbox::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool EntityMailbox::has_pending_tasks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !queue_.empty();
}

std::size_t EntityMailbox::max_depth() const {
    return options_.max_depth;
}

std::uint64_t EntityMailbox::dropped_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_count_;
}

}  // namespace runtime::entity

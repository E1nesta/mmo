#include "runtime/entity/entity_mailbox.h"

#include <utility>

namespace runtime::entity {

EntityMailbox::EntityMailbox(EntityMailboxOptions options)
    : options_(options) {}

bool EntityMailbox::try_push(EntityMessage message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.size() >= options_.max_depth) {
        ++dropped_count_;
        return false;
    }
    queue_.push_back(std::move(message));
    return true;
}

std::optional<EntityMessage> EntityMailbox::pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        return std::nullopt;
    }
    auto message = std::move(queue_.front());
    queue_.pop_front();
    return message;
}

std::size_t EntityMailbox::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

std::size_t EntityMailbox::max_depth() const {
    return options_.max_depth;
}

std::uint64_t EntityMailbox::dropped_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dropped_count_;
}

}  // namespace runtime::entity

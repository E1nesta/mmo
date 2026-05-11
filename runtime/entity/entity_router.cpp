#include "runtime/entity/entity_router.h"

#include <utility>

namespace runtime::entity {

std::shared_ptr<EntityMailbox> EntityRouter::bind(
    EntityId entity_id,
    EntityMailboxOptions options) {
    if (!is_valid_entity_id(entity_id)) {
        return nullptr;
    }
    auto mailbox = std::make_shared<EntityMailbox>(options);
    return bind(entity_id, mailbox) ? mailbox : nullptr;
}

bool EntityRouter::bind(
    EntityId entity_id,
    std::shared_ptr<EntityMailbox> mailbox) {
    if (mailbox == nullptr) {
        return false;
    }
    if (!is_valid_entity_id(entity_id)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return mailboxes_.emplace(entity_id, std::move(mailbox)).second;
}

std::shared_ptr<EntityMailbox> EntityRouter::find_or_bind(
    EntityId entity_id,
    EntityMailboxOptions options) {
    if (!is_valid_entity_id(entity_id)) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = mailboxes_.find(entity_id);
    if (it != mailboxes_.end()) {
        return it->second;
    }
    auto mailbox = std::make_shared<EntityMailbox>(options);
    auto inserted = mailboxes_.emplace(entity_id, mailbox);
    return inserted.first->second;
}

std::shared_ptr<EntityMailbox> EntityRouter::find(EntityId entity_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = mailboxes_.find(entity_id);
    return it == mailboxes_.end() ? nullptr : it->second;
}

void EntityRouter::unbind(EntityId entity_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    mailboxes_.erase(entity_id);
}

std::size_t EntityRouter::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mailboxes_.size();
}

std::optional<EntityId> entity_id_from_route_key(
    EntityKind kind,
    std::uint64_t route_key) {
    if (route_key == 0) {
        return std::nullopt;
    }
    return EntityId{kind, route_key};
}

}  // namespace runtime::entity

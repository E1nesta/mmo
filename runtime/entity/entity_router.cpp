#include "runtime/entity/entity_router.h"

#include <utility>

namespace runtime::entity {

std::shared_ptr<EntityMailbox> EntityRouter::bind(
    EntityId entity_id,
    EntityMailboxOptions options) {
    auto mailbox = std::make_shared<EntityMailbox>(options);
    bind(entity_id, mailbox);
    return mailbox;
}

bool EntityRouter::bind(
    EntityId entity_id,
    std::shared_ptr<EntityMailbox> mailbox) {
    if (mailbox == nullptr) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    mailboxes_[entity_id] = std::move(mailbox);
    return true;
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

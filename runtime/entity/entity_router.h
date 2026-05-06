#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "runtime/entity/entity_id.h"
#include "runtime/entity/entity_mailbox.h"

namespace runtime::entity {

class EntityRouter {
public:
    std::shared_ptr<EntityMailbox> bind(
        EntityId entity_id,
        EntityMailboxOptions options = {});
    bool bind(EntityId entity_id, std::shared_ptr<EntityMailbox> mailbox);
    std::shared_ptr<EntityMailbox> find(EntityId entity_id) const;
    void unbind(EntityId entity_id);
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<EntityId, std::shared_ptr<EntityMailbox>, EntityIdHash>
        mailboxes_;
};

std::optional<EntityId> entity_id_from_route_key(
    EntityKind kind,
    std::uint64_t route_key);

}  // namespace runtime::entity

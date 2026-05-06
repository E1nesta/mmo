#pragma once

#include <cstddef>
#include <functional>

#include "runtime/entity/entity_mailbox.h"
#include "runtime/entity/entity_router.h"

namespace runtime::entity {

class EntityExecutor {
public:
    using Handler = std::function<void(const EntityMessage&)>;

    explicit EntityExecutor(EntityRouter& router);

    bool post(EntityMessage message);
    std::size_t drain(EntityId entity_id, std::size_t max_messages, Handler handler);

private:
    EntityRouter& router_;
};

}  // namespace runtime::entity

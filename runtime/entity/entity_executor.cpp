#include "runtime/entity/entity_executor.h"

#include <utility>

namespace runtime::entity {

EntityExecutor::EntityExecutor(EntityRouter& router)
    : router_(router) {}

bool EntityExecutor::post(EntityMessage message) {
    auto mailbox = router_.find(message.entity_id);
    if (mailbox == nullptr) {
        mailbox = router_.bind(message.entity_id);
    }
    return mailbox->try_push(std::move(message));
}

std::size_t EntityExecutor::drain(
    EntityId entity_id,
    std::size_t max_messages,
    Handler handler) {
    auto mailbox = router_.find(entity_id);
    if (mailbox == nullptr || !handler) {
        return 0;
    }

    std::size_t handled = 0;
    while (handled < max_messages) {
        auto message = mailbox->pop();
        if (!message.has_value()) {
            break;
        }
        handler(*message);
        ++handled;
    }
    return handled;
}

}  // namespace runtime::entity

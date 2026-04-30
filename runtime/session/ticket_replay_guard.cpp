#include "runtime/session/ticket_replay_guard.h"

namespace mmo::runtime::session {

bool TicketReplayGuard::consume(
    const std::string& ticket_id,
    std::uint64_t now_millis,
    std::uint64_t expire_at_millis) {
    if (ticket_id.empty() || expire_at_millis <= now_millis) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = consumed_ticket_ids_.begin();
         it != consumed_ticket_ids_.end();) {
        if (it->second <= now_millis) {
            it = consumed_ticket_ids_.erase(it);
        } else {
            ++it;
        }
    }
    if (consumed_ticket_ids_.find(ticket_id) != consumed_ticket_ids_.end()) {
        return false;
    }
    consumed_ticket_ids_[ticket_id] = expire_at_millis;
    return true;
}

void TicketReplayGuard::purge_expired(std::uint64_t now_millis) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = consumed_ticket_ids_.begin();
         it != consumed_ticket_ids_.end();) {
        if (it->second <= now_millis) {
            it = consumed_ticket_ids_.erase(it);
        } else {
            ++it;
        }
    }
}

std::size_t TicketReplayGuard::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return consumed_ticket_ids_.size();
}

}  // namespace mmo::runtime::session

#include "runtime/session/session_context.h"

namespace mmo::runtime::session {

bool HeartbeatState::expired(std::uint64_t now_millis) const {
    return timeout_millis > 0 && now_millis > last_seen_millis + timeout_millis;
}

bool ReconnectTicket::valid(std::uint64_t now_millis) const {
    return expire_at_millis > 0 && now_millis <= expire_at_millis;
}

ConnectionBinding SessionRegistry::bind(
    std::int64_t player_id,
    const std::string& session_token,
    std::uint64_t now_millis) {
    std::lock_guard<std::mutex> lock(mutex_);
    ConnectionBinding binding;
    binding.connection_id = next_connection_id_++;
    binding.player_id = player_id;
    binding.session_token = session_token;
    binding.last_seen_millis = now_millis;
    binding.authenticated = true;
    bindings_[player_id] = binding;
    reconnect_tickets_.erase(player_id);
    return binding;
}

bool SessionRegistry::is_bound(
    std::int64_t player_id,
    const std::string& session_token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = bindings_.find(player_id);
    return it != bindings_.end() && it->second.session_token == session_token;
}

bool SessionRegistry::touch(std::int64_t player_id, std::uint64_t now_millis) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = bindings_.find(player_id);
    if (it == bindings_.end()) {
        return false;
    }
    it->second.last_seen_millis = now_millis;
    return true;
}

std::optional<ConnectionBinding> SessionRegistry::find(
    std::int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = bindings_.find(player_id);
    if (it == bindings_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<ReconnectTicket> SessionRegistry::make_reconnect_ticket(
    std::int64_t player_id,
    std::uint64_t now_millis,
    std::uint64_t ttl_millis) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = bindings_.find(player_id);
    if (it == bindings_.end()) {
        return std::nullopt;
    }

    ReconnectTicket ticket;
    ticket.connection_id = it->second.connection_id;
    ticket.player_id = it->second.player_id;
    ticket.session_token = it->second.session_token;
    ticket.expire_at_millis = now_millis + ttl_millis;
    reconnect_tickets_[player_id] = ticket;
    return ticket;
}

bool SessionRegistry::can_reconnect(
    std::int64_t player_id,
    const std::string& session_token,
    std::uint64_t now_millis) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = reconnect_tickets_.find(player_id);
    return it != reconnect_tickets_.end() &&
           it->second.session_token == session_token &&
           it->second.valid(now_millis);
}

void SessionRegistry::unbind(std::int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    bindings_.erase(player_id);
}

}  // namespace mmo::runtime::session

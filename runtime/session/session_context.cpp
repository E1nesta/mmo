#include "runtime/session/session_context.h"

namespace mmo::runtime::session {

ConnectionBinding SessionRegistry::bind(
    std::int64_t player_id,
    const std::string& session_token) {
    std::lock_guard<std::mutex> lock(mutex_);
    ConnectionBinding binding;
    binding.connection_id = next_connection_id_++;
    binding.player_id = player_id;
    binding.session_token = session_token;
    binding.authenticated = true;
    bindings_[player_id] = binding;
    return binding;
}

bool SessionRegistry::is_bound(
    std::int64_t player_id,
    const std::string& session_token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = bindings_.find(player_id);
    return it != bindings_.end() && it->second.session_token == session_token;
}

void SessionRegistry::unbind(std::int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    bindings_.erase(player_id);
}

}  // namespace mmo::runtime::session

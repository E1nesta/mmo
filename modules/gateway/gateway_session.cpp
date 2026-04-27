#include "modules/gateway/gateway_session.h"

namespace mmo::modules::gateway {

SessionBinding GatewaySessionRegistry::bind(
    std::int64_t player_id,
    const std::string& session_token) {
    SessionBinding binding;
    binding.player_id = player_id;
    binding.session_token = session_token;
    binding.connection_id = next_connection_id_++;
    bindings_[player_id] = binding;
    return binding;
}

bool GatewaySessionRegistry::is_bound(
    std::int64_t player_id,
    const std::string& session_token) const {
    const auto it = bindings_.find(player_id);
    return it != bindings_.end() && it->second.session_token == session_token;
}

}  // namespace mmo::modules::gateway

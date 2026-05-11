#include "runtime/session/session_context.h"

#include <random>
#include <sstream>

namespace runtime::session {
namespace {

std::string make_game_session_id(
    std::uint64_t connection_id,
    std::int64_t player_id,
    std::uint64_t now_millis) {
    std::random_device random_device;
    std::mt19937_64 generator(random_device());
    std::uniform_int_distribution<std::uint64_t> distribution;
    std::ostringstream stream;
    stream << "gs-" << player_id << '-' << connection_id << '-'
           << now_millis << '-' << std::hex << distribution(generator);
    return stream.str();
}

}  // namespace

bool ConnectionBinding::valid(std::uint64_t now_millis) const {
    return authenticated && !game_session_id.empty() &&
           (expire_at_millis == 0 || now_millis <= expire_at_millis) &&
           (heartbeat_timeout_millis == 0 ||
            now_millis <= last_seen_millis + heartbeat_timeout_millis);
}

bool ReconnectTicket::valid(std::uint64_t now_millis) const {
    return expire_at_millis > 0 && now_millis <= expire_at_millis;
}

ConnectionBinding SessionRegistry::bind(
    std::int64_t player_id,
    const std::string& session_token,
    std::uint64_t now_millis) {
    return bind(0, player_id, session_token, {}, {}, now_millis, 0);
}

ConnectionBinding SessionRegistry::bind(
    std::int64_t account_id,
    std::int64_t player_id,
    const std::string& session_token,
    const std::string& gateway_id,
    const std::string& device_id,
    std::uint64_t now_millis,
    std::uint64_t expire_at_millis,
    std::uint64_t heartbeat_timeout_millis) {
    std::lock_guard<std::mutex> lock(mutex_);
    ConnectionBinding binding;
    binding.connection_id = next_connection_id_++;
    binding.game_session_id =
        make_game_session_id(binding.connection_id, player_id, now_millis);
    binding.account_id = account_id;
    binding.player_id = player_id;
    binding.session_token = session_token;
    binding.gateway_id = gateway_id;
    binding.device_id = device_id;
    binding.issued_at_millis = now_millis;
    binding.last_seen_millis = now_millis;
    binding.expire_at_millis = expire_at_millis;
    binding.heartbeat_timeout_millis = heartbeat_timeout_millis;
    binding.authenticated = true;
    const auto previous = bindings_.find(player_id);
    if (previous != bindings_.end()) {
        player_by_connection_id_.erase(previous->second.connection_id);
    }
    bindings_[player_id] = binding;
    player_by_connection_id_[binding.connection_id] = player_id;
    return binding;
}

bool SessionRegistry::is_bound(
    std::int64_t player_id,
    const std::string& session_token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = bindings_.find(player_id);
    return it != bindings_.end() && it->second.session_token == session_token &&
           (it->second.expire_at_millis == 0 ||
            it->second.valid(it->second.last_seen_millis));
}

bool SessionRegistry::is_bound(
    std::int64_t player_id,
    const std::string& session_token,
    const std::string& game_session_id,
    std::uint64_t now_millis) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = bindings_.find(player_id);
    return it != bindings_.end() &&
           it->second.session_token == session_token &&
           it->second.game_session_id == game_session_id &&
           it->second.valid(now_millis);
}

bool SessionRegistry::touch(std::int64_t player_id, std::uint64_t now_millis) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = bindings_.find(player_id);
    if (it == bindings_.end() || !it->second.valid(now_millis)) {
        return false;
    }
    it->second.last_seen_millis = now_millis;
    return true;
}

bool SessionRegistry::touch(
    std::int64_t player_id,
    const std::string& game_session_id,
    std::uint64_t now_millis) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = bindings_.find(player_id);
    if (it == bindings_.end() ||
        it->second.game_session_id != game_session_id ||
        !it->second.valid(now_millis)) {
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

std::optional<ConnectionBinding> SessionRegistry::find_by_connection_id(
    std::uint64_t connection_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto player_it = player_by_connection_id_.find(connection_id);
    if (player_it == player_by_connection_id_.end()) {
        return std::nullopt;
    }
    const auto binding_it = bindings_.find(player_it->second);
    if (binding_it == bindings_.end()) {
        return std::nullopt;
    }
    return binding_it->second;
}

ConnectionBinding SessionRegistry::reconnect(
    std::int64_t account_id,
    std::int64_t player_id,
    const std::string& session_token,
    const std::string& game_session_id,
    const std::string& gateway_id,
    const std::string& device_id,
    std::uint64_t now_millis,
    std::uint64_t expire_at_millis,
    std::uint64_t heartbeat_timeout_millis) {
    std::lock_guard<std::mutex> lock(mutex_);
    ConnectionBinding binding;
    binding.connection_id = next_connection_id_++;
    binding.game_session_id = game_session_id;
    binding.account_id = account_id;
    binding.player_id = player_id;
    binding.session_token = session_token;
    binding.gateway_id = gateway_id;
    binding.device_id = device_id;
    binding.issued_at_millis = now_millis;
    binding.last_seen_millis = now_millis;
    binding.expire_at_millis = expire_at_millis;
    binding.heartbeat_timeout_millis = heartbeat_timeout_millis;
    binding.authenticated = true;
    const auto previous = bindings_.find(player_id);
    if (previous != bindings_.end()) {
        player_by_connection_id_.erase(previous->second.connection_id);
    }
    bindings_[player_id] = binding;
    player_by_connection_id_[binding.connection_id] = player_id;
    return binding;
}

void SessionRegistry::unbind(std::int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = bindings_.find(player_id);
    if (it != bindings_.end()) {
        player_by_connection_id_.erase(it->second.connection_id);
    }
    bindings_.erase(player_id);
}

}  // namespace runtime::session

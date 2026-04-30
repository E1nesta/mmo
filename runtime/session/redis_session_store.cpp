#include "runtime/session/redis_session_store.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

#include "runtime/storage/redis_keys.h"

namespace mmo::runtime::session {
namespace {

std::uint64_t current_time_millis() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

void append_field(const std::string& value, std::string* output) {
    output->append(std::to_string(value.size()));
    output->push_back(':');
    output->append(value);
}

bool read_field(
    const std::string& payload,
    std::size_t* offset,
    std::string* value) {
    const auto colon = payload.find(':', *offset);
    if (colon == std::string::npos || colon == *offset) {
        return false;
    }
    std::size_t length = 0;
    try {
        length = static_cast<std::size_t>(
            std::stoull(payload.substr(*offset, colon - *offset)));
    } catch (...) {
        return false;
    }
    const auto begin = colon + 1U;
    if (begin + length > payload.size()) {
        return false;
    }
    *value = payload.substr(begin, length);
    *offset = begin + length;
    return true;
}

std::string serialize_binding(const ConnectionBinding& binding) {
    std::string output;
    append_field(binding.game_session_id, &output);
    append_field(std::to_string(binding.connection_id), &output);
    append_field(std::to_string(binding.account_id), &output);
    append_field(std::to_string(binding.player_id), &output);
    append_field(binding.session_token, &output);
    append_field(binding.gateway_id, &output);
    append_field(binding.device_id, &output);
    append_field(std::to_string(binding.issued_at_millis), &output);
    append_field(std::to_string(binding.last_seen_millis), &output);
    append_field(std::to_string(binding.expire_at_millis), &output);
    append_field(std::to_string(binding.heartbeat_timeout_millis), &output);
    append_field(binding.authenticated ? "1" : "0", &output);
    return output;
}

bool parse_binding(const std::string& payload, ConnectionBinding* binding) {
    std::vector<std::string> fields(12);
    std::size_t offset = 0;
    for (auto& field : fields) {
        if (!read_field(payload, &offset, &field)) {
            return false;
        }
    }
    if (offset != payload.size()) {
        return false;
    }
    try {
        binding->game_session_id = fields[0];
        binding->connection_id = std::stoull(fields[1]);
        binding->account_id = std::stoll(fields[2]);
        binding->player_id = std::stoll(fields[3]);
        binding->session_token = fields[4];
        binding->gateway_id = fields[5];
        binding->device_id = fields[6];
        binding->issued_at_millis = std::stoull(fields[7]);
        binding->last_seen_millis = std::stoull(fields[8]);
        binding->expire_at_millis = std::stoull(fields[9]);
        binding->heartbeat_timeout_millis = std::stoull(fields[10]);
        binding->authenticated = fields[11] == "1";
    } catch (...) {
        return false;
    }
    return true;
}

std::string serialize_online(const OnlineBinding& online) {
    std::string output;
    append_field(std::to_string(online.player_id), &output);
    append_field(online.game_session_id, &output);
    append_field(online.gateway_id, &output);
    return output;
}

bool parse_online(const std::string& payload, OnlineBinding* online) {
    std::vector<std::string> fields(3);
    std::size_t offset = 0;
    for (auto& field : fields) {
        if (!read_field(payload, &offset, &field)) {
            return false;
        }
    }
    if (offset != payload.size()) {
        return false;
    }
    try {
        online->player_id = std::stoll(fields[0]);
        online->game_session_id = fields[1];
        online->gateway_id = fields[2];
    } catch (...) {
        return false;
    }
    return true;
}

std::uint64_t ttl_millis_for(
    const ConnectionBinding& binding,
    std::uint64_t now_millis) {
    std::uint64_t deadline = binding.expire_at_millis == 0
                                 ? std::numeric_limits<std::uint64_t>::max()
                                 : binding.expire_at_millis;
    if (binding.heartbeat_timeout_millis > 0) {
        deadline = std::min(
            deadline, binding.last_seen_millis + binding.heartbeat_timeout_millis);
    }
    if (deadline <= now_millis) {
        return 0;
    }
    return deadline - now_millis;
}

}  // namespace

RedisSessionStore::RedisSessionStore(
    std::shared_ptr<mmo::runtime::storage::RedisConnectionPool> pool)
    : pool_(std::move(pool)) {}

bool RedisSessionStore::save_binding(
    const ConnectionBinding& binding,
    std::string* error_message) {
    if (pool_ == nullptr || !binding.authenticated ||
        binding.game_session_id.empty() || binding.player_id <= 0) {
        if (error_message != nullptr) {
            *error_message = "redis session binding is invalid";
        }
        return false;
    }
    const auto now = current_time_millis();
    const auto ttl = ttl_millis_for(binding, now);
    if (ttl == 0) {
        if (error_message != nullptr) {
            *error_message = "redis session binding is expired";
        }
        return false;
    }
    const auto client = pool_->acquire();
    if (!client->set_with_ttl_millis(
            mmo::runtime::storage::game_session_key(binding.game_session_id),
            serialize_binding(binding),
            ttl,
            error_message)) {
        return false;
    }
    OnlineBinding online;
    online.player_id = binding.player_id;
    online.game_session_id = binding.game_session_id;
    online.gateway_id = binding.gateway_id;
    return client->set_with_ttl_millis(
        mmo::runtime::storage::online_key(binding.player_id),
        serialize_online(online),
        ttl,
        error_message);
}

bool RedisSessionStore::is_bound(
    std::int64_t player_id,
    const std::string& session_token,
    const std::string& game_session_id,
    std::uint64_t now_millis,
    std::string* error_message) const {
    const auto binding = load_binding(game_session_id, error_message);
    return binding.has_value() &&
           binding->player_id == player_id &&
           binding->session_token == session_token &&
           binding->game_session_id == game_session_id &&
           binding->valid(now_millis);
}

bool RedisSessionStore::touch_binding(
    std::int64_t player_id,
    const std::string& game_session_id,
    std::uint64_t now_millis,
    std::string* error_message) {
    auto binding = load_binding(game_session_id, error_message);
    if (!binding.has_value() || binding->player_id != player_id ||
        !binding->valid(now_millis)) {
        if (error_message != nullptr && error_message->empty()) {
            *error_message = "redis session binding is not valid";
        }
        return false;
    }
    binding->last_seen_millis = now_millis;
    return save_binding(*binding, error_message);
}

std::optional<OnlineBinding> RedisSessionStore::find_online(
    std::int64_t player_id,
    std::string* error_message) const {
    if (pool_ == nullptr || player_id <= 0) {
        if (error_message != nullptr) {
            *error_message = "redis session store is not initialized";
        }
        return std::nullopt;
    }
    const auto client = pool_->acquire();
    const auto value =
        client->get(mmo::runtime::storage::online_key(player_id), error_message);
    if (!value.has_value()) {
        return std::nullopt;
    }
    OnlineBinding online;
    if (!parse_online(*value, &online)) {
        if (error_message != nullptr) {
            *error_message = "redis online binding is malformed";
        }
        return std::nullopt;
    }
    return online;
}

std::optional<ConnectionBinding> RedisSessionStore::load_binding(
    const std::string& game_session_id,
    std::string* error_message) const {
    if (pool_ == nullptr || game_session_id.empty()) {
        if (error_message != nullptr) {
            *error_message = "redis session store is not initialized";
        }
        return std::nullopt;
    }
    const auto client = pool_->acquire();
    const auto value = client->get(
        mmo::runtime::storage::game_session_key(game_session_id), error_message);
    if (!value.has_value()) {
        return std::nullopt;
    }
    ConnectionBinding binding;
    if (!parse_binding(*value, &binding)) {
        if (error_message != nullptr) {
            *error_message = "redis session binding is malformed";
        }
        return std::nullopt;
    }
    return binding;
}

}  // namespace mmo::runtime::session

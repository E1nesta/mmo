#include "runtime/storage/redis_keys.h"

namespace mmo::runtime::storage {

std::string session_key(const std::string& session_token) {
    return "session:" + session_token;
}

std::string online_key(std::int64_t player_id) {
    return "online:" + std::to_string(player_id);
}

std::string player_cache_key(std::int64_t player_id) {
    return "player_cache:" + std::to_string(player_id);
}

std::string player_lock_key(std::int64_t player_id) {
    return "player_lock:" + std::to_string(player_id);
}

std::string instance_context_key(std::int64_t instance_id) {
    return "instance_context:" + std::to_string(instance_id);
}

}  // namespace mmo::runtime::storage

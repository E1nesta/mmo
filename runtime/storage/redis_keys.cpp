#include "runtime/storage/redis_keys.h"

namespace runtime::storage {

std::string game_session_key(const std::string& game_session_id) {
    return "game_session:" + game_session_id;
}

std::string reconnect_ticket_key(const std::string& ticket_id) {
    return "reconnect_ticket:" + ticket_id;
}

std::string ticket_replay_key(const std::string& ticket_id) {
    return "ticket_replay:" + ticket_id;
}

std::string online_key(std::int64_t player_id) {
    return "online:" + std::to_string(player_id);
}

}  // namespace runtime::storage

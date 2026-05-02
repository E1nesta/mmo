#pragma once

#include <cstdint>
#include <string>

namespace mmo::runtime::storage {

std::string session_key(const std::string& session_token);
std::string game_session_key(const std::string& game_session_id);
std::string reconnect_ticket_key(const std::string& ticket_id);
std::string ticket_replay_key(const std::string& ticket_id);
std::string online_key(std::int64_t player_id);
std::string player_cache_key(std::int64_t player_id);
std::string player_lock_key(std::int64_t player_id);
std::string instance_context_key(std::int64_t instance_id);

}  // namespace mmo::runtime::storage

#pragma once

#include <cstdint>
#include <string>

namespace runtime::storage {

std::string game_session_key(const std::string& game_session_id);
std::string reconnect_ticket_key(const std::string& ticket_id);
std::string ticket_replay_key(const std::string& ticket_id);
std::string online_key(std::int64_t player_id);

}  // namespace runtime::storage

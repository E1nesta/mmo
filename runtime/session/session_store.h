#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "runtime/session/session_context.h"

namespace runtime::session {

struct OnlineBinding {
    std::int64_t player_id{};
    std::string game_session_id;
    std::string gateway_id;
};

class SessionStore {
public:
    virtual ~SessionStore() = default;

    virtual bool save_binding(
        const ConnectionBinding& binding,
        std::string* error_message) = 0;
    virtual bool is_bound(
        std::int64_t player_id,
        const std::string& session_token,
        const std::string& game_session_id,
        std::uint64_t now_millis,
        std::string* error_message) const = 0;
    virtual bool touch_binding(
        std::int64_t player_id,
        const std::string& game_session_id,
        std::uint64_t now_millis,
        std::string* error_message) = 0;
    virtual std::optional<OnlineBinding> find_online(
        std::int64_t player_id,
        std::string* error_message) const = 0;
    virtual bool save_reconnect_ticket(
        const std::string& ticket_id,
        const ReconnectTicket& ticket,
        std::uint64_t now_millis,
        std::string* error_message) = 0;
    virtual bool consume_reconnect_ticket(
        const std::string& ticket_id,
        std::uint64_t now_millis,
        ReconnectTicket* ticket,
        std::string* error_message) = 0;
};

}  // namespace runtime::session

#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace mmo::runtime::session {

struct RequestContext {
    std::uint64_t request_id{};
    std::int64_t player_id{};
    std::string session_token;
    std::string message_type;
};

struct ConnectionBinding {
    std::string game_session_id;
    std::uint64_t connection_id{};
    std::int64_t account_id{};
    std::int64_t player_id{};
    std::string session_token;
    std::string gateway_id;
    std::string device_id;
    std::uint64_t issued_at_millis{};
    std::uint64_t last_seen_millis{};
    std::uint64_t expire_at_millis{};
    std::uint64_t heartbeat_timeout_millis{};
    bool authenticated{};

    bool valid(std::uint64_t now_millis) const;
};

struct HeartbeatState {
    std::uint64_t last_seen_millis{};
    std::uint64_t timeout_millis{};
    std::uint32_t missed_count{};

    bool expired(std::uint64_t now_millis) const;
};

struct ReconnectTicket {
    std::int64_t account_id{};
    std::uint64_t connection_id{};
    std::string game_session_id;
    std::int64_t player_id{};
    std::string session_token;
    std::string gateway_id;
    std::string device_id;
    std::uint64_t expire_at_millis{};

    bool valid(std::uint64_t now_millis) const;
};

class SessionRegistry {
public:
    ConnectionBinding bind(
        std::int64_t player_id,
        const std::string& session_token,
        std::uint64_t now_millis = 0);
    ConnectionBinding bind(
        std::int64_t account_id,
        std::int64_t player_id,
        const std::string& session_token,
        const std::string& gateway_id,
        const std::string& device_id,
        std::uint64_t now_millis,
        std::uint64_t expire_at_millis,
        std::uint64_t heartbeat_timeout_millis = 0);
    bool is_bound(std::int64_t player_id, const std::string& session_token) const;
    bool is_bound(
        std::int64_t player_id,
        const std::string& session_token,
        const std::string& game_session_id,
        std::uint64_t now_millis) const;
    bool touch(std::int64_t player_id, std::uint64_t now_millis);
    bool touch(
        std::int64_t player_id,
        const std::string& game_session_id,
        std::uint64_t now_millis);
    std::optional<ConnectionBinding> find(std::int64_t player_id) const;
    std::optional<ReconnectTicket> make_reconnect_ticket(
        std::int64_t player_id,
        std::uint64_t now_millis,
        std::uint64_t ttl_millis);
    ConnectionBinding reconnect(
        std::int64_t account_id,
        std::int64_t player_id,
        const std::string& session_token,
        const std::string& game_session_id,
        const std::string& gateway_id,
        const std::string& device_id,
        std::uint64_t now_millis,
        std::uint64_t expire_at_millis,
        std::uint64_t heartbeat_timeout_millis = 0);
    bool can_reconnect(
        std::int64_t player_id,
        const std::string& session_token,
        const std::string& game_session_id,
        std::uint64_t now_millis) const;
    void unbind(std::int64_t player_id);

private:
    mutable std::mutex mutex_;
    std::uint64_t next_connection_id_{1};
    std::unordered_map<std::int64_t, ConnectionBinding> bindings_;
    std::unordered_map<std::int64_t, ReconnectTicket> reconnect_tickets_;
};

}  // namespace mmo::runtime::session

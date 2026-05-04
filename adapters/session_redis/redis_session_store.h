#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "runtime/session/session_store.h"
#include "runtime/storage/redis_connection_pool.h"

namespace mmo::adapters::session_redis {

using ConnectionBinding = mmo::runtime::session::ConnectionBinding;
using OnlineBinding = mmo::runtime::session::OnlineBinding;
using ReconnectTicket = mmo::runtime::session::ReconnectTicket;

class RedisSessionStore final : public mmo::runtime::session::SessionStore {
public:
    explicit RedisSessionStore(
        std::shared_ptr<mmo::runtime::storage::RedisConnectionPool> pool);

    bool save_binding(
        const ConnectionBinding& binding,
        std::string* error_message) override;
    bool is_bound(
        std::int64_t player_id,
        const std::string& session_token,
        const std::string& game_session_id,
        std::uint64_t now_millis,
        std::string* error_message) const override;
    bool touch_binding(
        std::int64_t player_id,
        const std::string& game_session_id,
        std::uint64_t now_millis,
        std::string* error_message) override;
    std::optional<OnlineBinding> find_online(
        std::int64_t player_id,
        std::string* error_message) const override;
    bool save_reconnect_ticket(
        const std::string& ticket_id,
        const ReconnectTicket& ticket,
        std::uint64_t now_millis,
        std::string* error_message) override;
    bool consume_reconnect_ticket(
        const std::string& ticket_id,
        std::uint64_t now_millis,
        ReconnectTicket* ticket,
        std::string* error_message) override;

private:
    std::optional<ConnectionBinding> load_binding(
        const std::string& game_session_id,
        std::string* error_message) const;

    std::shared_ptr<mmo::runtime::storage::RedisConnectionPool> pool_;
};

}  // namespace mmo::adapters::session_redis

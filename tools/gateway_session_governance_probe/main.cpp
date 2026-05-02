#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "runtime/foundation/server_config.h"
#include "runtime/session/redis_session_store.h"
#include "runtime/session/session_context.h"
#include "runtime/storage/storage_bootstrap.h"

namespace {

std::uint64_t now_millis() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

mmo::runtime::session::ConnectionBinding make_binding(
    std::int64_t player_id,
    const std::string& session_token,
    const std::string& game_session_id,
    std::uint64_t connection_id,
    std::uint64_t now) {
    mmo::runtime::session::ConnectionBinding binding;
    binding.connection_id = connection_id;
    binding.game_session_id = game_session_id;
    binding.account_id = 1098216;
    binding.player_id = player_id;
    binding.session_token = session_token;
    binding.gateway_id = "game_gateway_server-local";
    binding.device_id = "gateway-session-governance-probe";
    binding.issued_at_millis = now;
    binding.last_seen_millis = now;
    binding.expire_at_millis = now + 120000;
    binding.heartbeat_timeout_millis = 30000;
    binding.authenticated = true;
    return binding;
}

}  // namespace

int main() {
    const auto config = mmo::runtime::foundation::load_server_config_from_env();
    std::string error;
    std::shared_ptr<mmo::runtime::storage::RedisConnectionPool> redis_pool;
    assert(mmo::runtime::storage::initialize_redis_pool(
        config, &redis_pool, &error));

    mmo::runtime::session::RedisSessionStore redis_sessions(redis_pool);
    const auto now = now_millis();
    const auto player_id =
        static_cast<std::int64_t>(880000000LL + (now % 1000000ULL));

    const auto old_binding = make_binding(
        player_id, "session-old", "gs-old-" + std::to_string(now), 1, now);
    assert(redis_sessions.save_binding(old_binding, &error));
    assert(redis_sessions.is_bound(
        player_id,
        "session-old",
        old_binding.game_session_id,
        now + 1,
        &error));

    const auto new_binding = make_binding(
        player_id, "session-new", "gs-new-" + std::to_string(now), 2, now + 2);
    assert(redis_sessions.save_binding(new_binding, &error));
    assert(!redis_sessions.is_bound(
        player_id,
        "session-old",
        old_binding.game_session_id,
        now + 3,
        &error));
    assert(redis_sessions.is_bound(
        player_id,
        "session-new",
        new_binding.game_session_id,
        now + 3,
        &error));

    mmo::runtime::session::ReconnectTicket reconnect_ticket;
    reconnect_ticket.account_id = new_binding.account_id;
    reconnect_ticket.connection_id = new_binding.connection_id;
    reconnect_ticket.game_session_id = new_binding.game_session_id;
    reconnect_ticket.player_id = new_binding.player_id;
    reconnect_ticket.session_token = new_binding.session_token;
    reconnect_ticket.gateway_id = new_binding.gateway_id;
    reconnect_ticket.device_id = new_binding.device_id;
    reconnect_ticket.expire_at_millis = now + 60000;

    const std::string ticket_id = "reconnect-probe-" + std::to_string(now);
    assert(redis_sessions.save_reconnect_ticket(
        ticket_id, reconnect_ticket, now, &error));

    mmo::runtime::session::ReconnectTicket consumed;
    assert(redis_sessions.consume_reconnect_ticket(
        ticket_id, now + 1, &consumed, &error));
    assert(consumed.game_session_id == reconnect_ticket.game_session_id);
    assert(!redis_sessions.consume_reconnect_ticket(
        ticket_id, now + 2, &consumed, &error));

    mmo::runtime::session::SessionRegistry registry;
    const auto initial = registry.bind(
        new_binding.account_id,
        new_binding.player_id,
        new_binding.session_token,
        new_binding.gateway_id,
        new_binding.device_id,
        now,
        now + 120000,
        30000);
    const auto restored = registry.reconnect(
        new_binding.account_id,
        new_binding.player_id,
        new_binding.session_token,
        initial.game_session_id,
        new_binding.gateway_id,
        "reconnected-device",
        now + 1000,
        now + 121000,
        30000);
    assert(restored.game_session_id == initial.game_session_id);
    assert(restored.connection_id != initial.connection_id);
    assert(registry.is_bound(
        new_binding.player_id,
        new_binding.session_token,
        initial.game_session_id,
        now + 1001));

    std::cout << "gateway session governance probe ok\n";
    return 0;
}

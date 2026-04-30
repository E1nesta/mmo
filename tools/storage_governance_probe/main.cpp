#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "modules/player/mysql_player_repository.h"
#include "modules/player/player_service.h"
#include "runtime/foundation/server_config.h"
#include "runtime/session/redis_session_store.h"
#include "runtime/session/redis_ticket_replay_store.h"
#include "runtime/session/session_context.h"
#include "runtime/storage/storage_bootstrap.h"

namespace {

std::int64_t now_millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::int64_t probe_player_id() {
    return 780000000LL + (now_millis() % 1000000LL);
}

}  // namespace

int main() {
    const auto config = mmo::runtime::foundation::load_server_config_from_env();
    std::string error;

    std::shared_ptr<mmo::runtime::storage::MysqlConnectionPool> mysql_pool;
    assert(mmo::runtime::storage::initialize_mysql_pool(
        config, &mysql_pool, &error));
    auto player_repository =
        std::make_shared<mmo::modules::player::MysqlPlayerRepository>(mysql_pool);
    mmo::modules::player::PlayerService player_service(player_repository);

    const auto player_id = probe_player_id();
    const std::string idempotency_key =
        "storage-governance-probe-" + std::to_string(now_millis());
    const std::vector<mmo::modules::player::Reward> rewards = {
        {"gold", 10},
        {"exp", 20},
    };

    const auto first =
        player_service.apply_reward(player_id, idempotency_key, rewards);
    assert(first.applied);
    assert(first.gold == 10);
    assert(first.exp == 20);

    const auto duplicate =
        player_service.apply_reward(player_id, idempotency_key, rewards);
    assert(!duplicate.applied);
    assert(duplicate.gold == 10);
    assert(duplicate.exp == 20);

    const auto profile = player_repository->load_profile(player_id);
    assert(profile.has_value());
    assert(profile->gold == 10);
    assert(profile->exp == 20);

    std::shared_ptr<mmo::runtime::storage::RedisConnectionPool> redis_pool;
    assert(mmo::runtime::storage::initialize_redis_pool(
        config, &redis_pool, &error));

    mmo::runtime::session::RedisTicketReplayStore replay_store(redis_pool);
    const auto ticket_now = static_cast<std::uint64_t>(now_millis());
    const auto ticket_expire = ticket_now + 60000;
    const std::string ticket_id =
        "storage-governance-ticket-" + std::to_string(ticket_now);
    assert(replay_store.consume(ticket_id, ticket_now, ticket_expire));
    assert(!replay_store.consume(ticket_id, ticket_now + 1, ticket_expire));

    mmo::runtime::session::RedisSessionStore session_store(redis_pool);
    mmo::runtime::session::ConnectionBinding binding;
    binding.connection_id = 42;
    binding.game_session_id =
        "storage-governance-session-" + std::to_string(ticket_now);
    binding.account_id = 1098216;
    binding.player_id = player_id;
    binding.session_token = "storage-governance-session-token";
    binding.gateway_id = "game_gateway_server";
    binding.device_id = "storage-governance-probe";
    binding.issued_at_millis = ticket_now;
    binding.last_seen_millis = ticket_now;
    binding.expire_at_millis = ticket_now + 120000;
    binding.heartbeat_timeout_millis = 30000;
    binding.authenticated = true;

    assert(session_store.save_binding(binding, &error));
    assert(session_store.is_bound(
        binding.player_id,
        binding.session_token,
        binding.game_session_id,
        ticket_now,
        &error));
    assert(session_store.touch_binding(
        binding.player_id,
        binding.game_session_id,
        ticket_now + 1000,
        &error));
    const auto online = session_store.find_online(binding.player_id, &error);
    assert(online.has_value());
    assert(online->game_session_id == binding.game_session_id);
    assert(online->gateway_id == binding.gateway_id);

    std::cout << "storage governance probe ok\n";
    return 0;
}

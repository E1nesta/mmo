#include <cassert>
#include <iostream>
#include <string>

#include "runtime/protocol/auth_tokens.h"
#include "runtime/session/session_context.h"
#include "runtime/session/ticket_replay_guard.h"

int main() {
    mmo::runtime::protocol::AuthTokenOptions options;
    options.issuer = "mmo-auth";
    options.access_audience = "api_gateway_server";
    options.gateway_audience = "game_gateway_server";
    options.active_key_id = "kid-active";
    options.active_shared_secret = "active-secret";
    options.previous_key_id = "kid-previous";
    options.previous_shared_secret = "previous-secret";
    options.previous_key_accept_millis = 120000;

    std::string token;
    std::int64_t expires_at = 0;
    std::string error;
    assert(mmo::runtime::protocol::issue_auth_token(
        mmo::runtime::protocol::AuthTokenPurpose::kGateway,
        1001,
        2002,
        "session-2002",
        100000,
        60000,
        options,
        &token,
        &expires_at,
        &error));

    mmo::runtime::protocol::AuthTokenClaims claims;
    assert(mmo::runtime::protocol::validate_auth_token(
        token,
        mmo::runtime::protocol::AuthTokenPurpose::kGateway,
        "game_gateway_server",
        options,
        100001,
        &claims,
        &error));
    assert(claims.version == "v1");
    assert(claims.key_id == "kid-active");
    assert(claims.issuer == "mmo-auth");
    assert(claims.audience == "game_gateway_server");
    assert(claims.issued_at_epoch_millis == 100000);
    assert(claims.expires_at_epoch_millis == expires_at);
    assert(!claims.jti.empty());

    assert(!mmo::runtime::protocol::validate_auth_token(
        token,
        mmo::runtime::protocol::AuthTokenPurpose::kGateway,
        "api_gateway_server",
        options,
        100001,
        &claims,
        &error));
    assert(!mmo::runtime::protocol::validate_auth_token(
        token,
        mmo::runtime::protocol::AuthTokenPurpose::kGateway,
        "game_gateway_server",
        options,
        170001,
        &claims,
        &error));

    auto previous_options = options;
    previous_options.active_key_id = options.previous_key_id;
    previous_options.active_shared_secret = options.previous_shared_secret;
    std::string previous_token;
    assert(mmo::runtime::protocol::issue_auth_token(
        mmo::runtime::protocol::AuthTokenPurpose::kGateway,
        1001,
        2002,
        "session-2002",
        100000,
        60000,
        previous_options,
        &previous_token,
        &expires_at,
        &error));
    assert(mmo::runtime::protocol::validate_auth_token(
        previous_token,
        mmo::runtime::protocol::AuthTokenPurpose::kGateway,
        "game_gateway_server",
        options,
        100001,
        &claims,
        &error));
    assert(claims.key_id == "kid-previous");

    mmo::runtime::session::TicketReplayGuard replay_guard;
    assert(replay_guard.consume(claims.jti, 100001, claims.expires_at_epoch_millis));
    assert(!replay_guard.consume(claims.jti, 100002, claims.expires_at_epoch_millis));

    mmo::runtime::session::SessionRegistry sessions;
    const auto binding = sessions.bind(
        1001,
        2002,
        "session-2002",
        "gateway-a",
        "device-a",
        100000,
        160000);
    assert(!binding.game_session_id.empty());
    assert(sessions.is_bound(
        2002,
        "session-2002",
        binding.game_session_id,
        120000));
    assert(!sessions.is_bound(
        2002,
        "session-2002",
        binding.game_session_id,
        170001));

    mmo::runtime::session::SessionRegistry heartbeat_sessions;
    const auto heartbeat_binding = heartbeat_sessions.bind(
        1001,
        2002,
        "heartbeat-session",
        "gateway-a",
        "device-a",
        100000,
        200000,
        30000);
    assert(heartbeat_sessions.is_bound(
        2002,
        "heartbeat-session",
        heartbeat_binding.game_session_id,
        129999));
    assert(heartbeat_sessions.touch(
        2002,
        heartbeat_binding.game_session_id,
        120000));
    assert(heartbeat_sessions.is_bound(
        2002,
        "heartbeat-session",
        heartbeat_binding.game_session_id,
        149999));
    assert(!heartbeat_sessions.is_bound(
        2002,
        "heartbeat-session",
        heartbeat_binding.game_session_id,
        150001));

    const auto replacement = sessions.bind(
        1001,
        2002,
        "session-2002",
        "gateway-a",
        "device-a",
        130000,
        190000);
    assert(replacement.game_session_id != binding.game_session_id);
    assert(!sessions.is_bound(
        2002,
        "session-2002",
        binding.game_session_id,
        130001));
    assert(sessions.is_bound(
        2002,
        "session-2002",
        replacement.game_session_id,
        130001));

    std::cout << "security governance probe ok\n";
    return 0;
}

#include "apps/game_gateway_server/gateway_session_handlers.h"

#include <cstdint>
#include <string>

#include "public/gateway.pb.h"
#include "runtime/protocol/auth_tokens.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/internal_auth.h"
#include "runtime/protocol/message_types.h"

namespace mmo::apps::game_gateway_server {
namespace {

mmo::runtime::protocol::AuthTokenOptions make_token_options(
    const mmo::runtime::foundation::GatewayTicketConfig& config) {
    mmo::runtime::protocol::AuthTokenOptions options;
    options.issuer = config.issuer;
    options.access_audience = config.access_audience;
    options.gateway_audience = config.gateway_audience;
    options.active_key_id = config.active_key_id;
    options.active_shared_secret = config.active_shared_secret;
    options.previous_key_id = config.previous_key_id;
    options.previous_shared_secret = config.previous_shared_secret;
    options.previous_key_accept_millis = config.previous_key_accept_millis;
    return options;
}

bool issue_reconnect_ticket(
    const mmo::runtime::foundation::ServerConfig& config,
    mmo::runtime::session::RedisSessionStore& redis_sessions,
    const mmo::runtime::session::ConnectionBinding& binding,
    std::uint64_t now_millis,
    std::string* reconnect_ticket,
    std::int64_t* reconnect_ticket_expires_at,
    std::string* error_message) {
    if (reconnect_ticket == nullptr || reconnect_ticket_expires_at == nullptr) {
        if (error_message != nullptr) {
            *error_message = "reconnect ticket output is null";
        }
        return false;
    }
    if (!mmo::runtime::protocol::issue_auth_token(
            mmo::runtime::protocol::AuthTokenPurpose::kReconnect,
            binding.account_id,
            binding.player_id,
            binding.session_token,
            static_cast<std::int64_t>(now_millis),
            config.security.gateway_session.reconnect_ticket_ttl_millis,
            make_token_options(config.security.gateway_ticket),
            reconnect_ticket,
            reconnect_ticket_expires_at,
            error_message)) {
        return false;
    }

    mmo::runtime::protocol::AuthTokenClaims claims;
    if (!mmo::runtime::protocol::validate_auth_token(
            *reconnect_ticket,
            mmo::runtime::protocol::AuthTokenPurpose::kReconnect,
            config.security.gateway_ticket.gateway_audience,
            make_token_options(config.security.gateway_ticket),
            static_cast<std::int64_t>(now_millis),
            &claims,
            error_message)) {
        return false;
    }

    mmo::runtime::session::ReconnectTicket redis_ticket;
    redis_ticket.account_id = binding.account_id;
    redis_ticket.connection_id = binding.connection_id;
    redis_ticket.game_session_id = binding.game_session_id;
    redis_ticket.player_id = binding.player_id;
    redis_ticket.session_token = binding.session_token;
    redis_ticket.gateway_id = binding.gateway_id;
    redis_ticket.device_id = binding.device_id;
    redis_ticket.expire_at_millis =
        static_cast<std::uint64_t>(*reconnect_ticket_expires_at);
    return redis_sessions.save_reconnect_ticket(
        claims.jti, redis_ticket, now_millis, error_message);
}

}  // namespace

std::optional<mmo::common::Envelope> validate_bound_public_request(
    const mmo::common::Envelope& envelope,
    const mmo::runtime::session::SessionRegistry& sessions,
    const mmo::runtime::session::RedisSessionStore& redis_sessions,
    const mmo::common::RequestContext& context,
    mmo::runtime::observability::MetricsRegistry* metrics) {
    if (envelope.player_id() != context.player_id() ||
        envelope.session_token() != context.session_token() ||
        envelope.game_session_id() != context.game_session_id()) {
        return mmo::runtime::protocol::make_error_envelope(
            envelope, 400, "request context does not match envelope");
    }
    const auto now_millis = static_cast<std::uint64_t>(
        mmo::runtime::protocol::current_time_millis());
    std::string redis_error;
    if (envelope.game_session_id().empty() ||
        !sessions.is_bound(
            envelope.player_id(),
            envelope.session_token(),
            envelope.game_session_id(),
            now_millis) ||
        !redis_sessions.is_bound(
            envelope.player_id(),
            envelope.session_token(),
            envelope.game_session_id(),
            now_millis,
            &redis_error)) {
        if (metrics != nullptr) {
            metrics->record_game_session_expired();
        }
        return mmo::runtime::protocol::make_error_envelope(
            envelope, 401, "game session is not bound to gateway");
    }
    return std::nullopt;
}

void register_gateway_session_handlers(
    mmo::runtime::routing::GatewayRouter& gateway_router,
    const mmo::runtime::foundation::ServerConfig& config,
    mmo::runtime::session::SessionRegistry& sessions,
    mmo::runtime::session::RedisTicketReplayStore& ticket_replay_guard,
    mmo::runtime::session::RedisSessionStore& redis_sessions,
    mmo::runtime::observability::MetricsRegistry& security_metrics) {
    gateway_router.on(
        mmo::runtime::protocol::kGateLoginRequest,
        [&sessions,
         &redis_sessions,
         &ticket_replay_guard,
         &security_metrics,
         &config](const mmo::common::Envelope& envelope) {
            mmo::public_api::GateLoginRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid gate login request");
            }
            if (envelope.player_id() != request.player_id() ||
                envelope.session_token() != request.session_token() ||
                request.context().player_id() != request.player_id() ||
                request.context().session_token() != request.session_token()) {
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "gate login context does not match envelope");
            }

            mmo::runtime::protocol::AuthTokenClaims claims;
            std::string token_error;
            if (!mmo::runtime::protocol::validate_auth_token(
                    request.gateway_ticket(),
                    mmo::runtime::protocol::AuthTokenPurpose::kGateway,
                    config.security.gateway_ticket.gateway_audience,
                    make_token_options(config.security.gateway_ticket),
                    mmo::runtime::protocol::current_time_millis(),
                    &claims,
                    &token_error)) {
                security_metrics.record_gateway_ticket_rejected();
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "gateway ticket authentication failed");
            }
            if (claims.player_id != request.player_id() ||
                claims.session_token != request.session_token()) {
                security_metrics.record_gateway_ticket_rejected();
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "gateway ticket does not match request");
            }
            const auto now_millis = static_cast<std::uint64_t>(
                mmo::runtime::protocol::current_time_millis());
            if (!ticket_replay_guard.consume(
                    claims.jti,
                    now_millis,
                    static_cast<std::uint64_t>(
                        claims.expires_at_epoch_millis))) {
                security_metrics.record_gateway_ticket_replay();
                security_metrics.record_gateway_ticket_rejected();
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "gateway ticket authentication failed");
            }

            const auto expires_at =
                now_millis +
                static_cast<std::uint64_t>(
                    config.security.gateway_session.game_session_ttl_millis);
            const auto binding = sessions.bind(
                claims.account_id,
                request.player_id(),
                request.session_token(),
                config.security.gateway_session.gateway_id,
                request.device_id(),
                now_millis,
                expires_at,
                static_cast<std::uint64_t>(
                    config.security.gateway_session.heartbeat_timeout_millis));
            std::string session_store_error;
            if (!redis_sessions.save_binding(binding, &session_store_error)) {
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 503, "gateway session store is unavailable");
            }
            std::string reconnect_ticket;
            std::int64_t reconnect_ticket_expires_at = 0;
            if (!issue_reconnect_ticket(
                    config,
                    redis_sessions,
                    binding,
                    now_millis,
                    &reconnect_ticket,
                    &reconnect_ticket_expires_at,
                    &session_store_error)) {
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 503, "reconnect ticket issue failed");
            }
            security_metrics.record_gate_login_success();

            mmo::public_api::GateLoginResponse response;
            auto response_context = request.context();
            response_context.set_game_session_id(binding.game_session_id);
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(response_context);
            response.set_bound(true);
            response.set_connection_id(binding.connection_id);
            response.set_game_session_id(binding.game_session_id);
            response.set_expires_at_epoch_millis(
                static_cast<std::int64_t>(binding.expire_at_millis));
            response.set_reconnect_ticket(reconnect_ticket);
            response.set_reconnect_ticket_expires_at_epoch_millis(
                reconnect_ticket_expires_at);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGateLoginResponse,
                response_context,
                response);
        });

    gateway_router.on(
        mmo::runtime::protocol::kReconnectRequest,
        [&sessions, &redis_sessions, &security_metrics, &config](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::ReconnectRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid reconnect request");
            }
            if (envelope.player_id() != request.player_id() ||
                envelope.session_token() != request.session_token() ||
                envelope.game_session_id() != request.game_session_id() ||
                request.context().player_id() != request.player_id() ||
                request.context().session_token() != request.session_token() ||
                request.context().game_session_id() != request.game_session_id()) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "reconnect context does not match envelope");
            }

            const auto now_millis = static_cast<std::uint64_t>(
                mmo::runtime::protocol::current_time_millis());
            mmo::runtime::protocol::AuthTokenClaims claims;
            std::string token_error;
            if (!mmo::runtime::protocol::validate_auth_token(
                    request.reconnect_ticket(),
                    mmo::runtime::protocol::AuthTokenPurpose::kReconnect,
                    config.security.gateway_ticket.gateway_audience,
                    make_token_options(config.security.gateway_ticket),
                    static_cast<std::int64_t>(now_millis),
                    &claims,
                    &token_error)) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "reconnect ticket authentication failed");
            }
            if (claims.player_id != request.player_id() ||
                claims.session_token != request.session_token()) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "reconnect ticket does not match request");
            }

            mmo::runtime::session::ReconnectTicket consumed_ticket;
            std::string session_store_error;
            if (!redis_sessions.consume_reconnect_ticket(
                    claims.jti,
                    now_millis,
                    &consumed_ticket,
                    &session_store_error) ||
                consumed_ticket.account_id != claims.account_id ||
                consumed_ticket.player_id != request.player_id() ||
                consumed_ticket.session_token != request.session_token() ||
                consumed_ticket.game_session_id != request.game_session_id() ||
                consumed_ticket.gateway_id !=
                    config.security.gateway_session.gateway_id) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "reconnect ticket authentication failed");
            }

            const auto expires_at =
                now_millis +
                static_cast<std::uint64_t>(
                    config.security.gateway_session.game_session_ttl_millis);
            const auto binding = sessions.reconnect(
                consumed_ticket.account_id,
                request.player_id(),
                request.session_token(),
                request.game_session_id(),
                config.security.gateway_session.gateway_id,
                request.device_id().empty()
                    ? consumed_ticket.device_id
                    : request.device_id(),
                now_millis,
                expires_at,
                static_cast<std::uint64_t>(
                    config.security.gateway_session.heartbeat_timeout_millis));
            if (!redis_sessions.save_binding(binding, &session_store_error)) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 503, "gateway session store is unavailable");
            }

            std::string next_reconnect_ticket;
            std::int64_t next_reconnect_ticket_expires_at = 0;
            if (!issue_reconnect_ticket(
                    config,
                    redis_sessions,
                    binding,
                    now_millis,
                    &next_reconnect_ticket,
                    &next_reconnect_ticket_expires_at,
                    &session_store_error)) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 503, "reconnect ticket issue failed");
            }
            security_metrics.record_reconnect_success();

            mmo::public_api::ReconnectResponse response;
            auto response_context = request.context();
            response_context.set_game_session_id(binding.game_session_id);
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(response_context);
            response.set_reconnected(true);
            response.set_connection_id(binding.connection_id);
            response.set_game_session_id(binding.game_session_id);
            response.set_expires_at_epoch_millis(
                static_cast<std::int64_t>(binding.expire_at_millis));
            response.set_reconnect_ticket(next_reconnect_ticket);
            response.set_reconnect_ticket_expires_at_epoch_millis(
                next_reconnect_ticket_expires_at);
            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kReconnectResponse,
                response_context,
                response);
        });

    gateway_router.on(
        mmo::runtime::protocol::kPingRequest,
        [&sessions, &redis_sessions, &security_metrics, &config](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::PingRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid ping request");
            }
            const auto validation_error =
                validate_bound_public_request(
                    envelope,
                    sessions,
                    redis_sessions,
                    request.context(),
                    &security_metrics);
            if (validation_error.has_value()) {
                return *validation_error;
            }
            const auto now_millis = static_cast<std::uint64_t>(
                mmo::runtime::protocol::current_time_millis());
            if (!sessions.touch(
                    envelope.player_id(), envelope.game_session_id(), now_millis)) {
                security_metrics.record_game_session_expired();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "game session is not bound to gateway");
            }
            std::string session_store_error;
            if (!redis_sessions.touch_binding(
                    envelope.player_id(),
                    envelope.game_session_id(),
                    now_millis,
                    &session_store_error)) {
                security_metrics.record_game_session_expired();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "game session is not bound to gateway");
            }
            const auto binding = sessions.find(envelope.player_id());
            if (!binding.has_value()) {
                security_metrics.record_game_session_expired();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "game session is not bound to gateway");
            }
            std::string reconnect_ticket;
            std::int64_t reconnect_ticket_expires_at = 0;
            if (!issue_reconnect_ticket(
                    config,
                    redis_sessions,
                    *binding,
                    now_millis,
                    &reconnect_ticket,
                    &reconnect_ticket_expires_at,
                    &session_store_error)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 503, "reconnect ticket issue failed");
            }

            mmo::public_api::PingResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_server_time_ms(static_cast<std::int64_t>(now_millis));
            response.set_reconnect_ticket(reconnect_ticket);
            response.set_reconnect_ticket_expires_at_epoch_millis(
                reconnect_ticket_expires_at);
            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kPingResponse,
                request.context(),
                response);
        });
}

}  // namespace mmo::apps::game_gateway_server

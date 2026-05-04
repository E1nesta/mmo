#include "apps/game_gateway_server/gateway_gate_login_handler.h"

#include <cstdint>
#include <string>

#include "public/gateway.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/internal_auth.h"
#include "runtime/protocol/message_types.h"

namespace mmo::apps::game_gateway_server {

void register_gateway_gate_login_handler(
    mmo::runtime::gateway::GatewayRouter& gateway_router,
    mmo::runtime::gateway::GatewaySessionContext& context) {
    auto& sessions = context.sessions;
    auto& session_store = context.session_store;
    auto& ticket_replay_guard = context.ticket_replay_guard;
    auto& security_metrics = context.security_metrics;
    const auto& config = context.config;
    gateway_router.on(
        mmo::runtime::protocol::kGateLoginRequest,
        [&sessions,
         &session_store,
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
                    mmo::runtime::gateway::make_gateway_token_options(config.security.gateway_ticket),
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
            if (!session_store.save_binding(binding, &session_store_error)) {
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 503, "gateway session store is unavailable");
            }
            std::string reconnect_ticket;
            std::int64_t reconnect_ticket_expires_at = 0;
            if (!mmo::runtime::gateway::issue_reconnect_ticket(
                    config,
                    session_store,
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
}

}  // namespace mmo::apps::game_gateway_server

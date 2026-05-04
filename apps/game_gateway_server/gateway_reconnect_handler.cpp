#include "apps/game_gateway_server/gateway_reconnect_handler.h"

#include <cstdint>
#include <string>

#include "public/gateway.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/internal_auth.h"
#include "apps/protocol/message_types.h"

namespace apps::game_gateway_server {

void register_gateway_reconnect_handler(
    runtime::gateway::GatewayRouter& gateway_router,
    runtime::gateway::GatewaySessionContext& context) {
    auto& sessions = context.sessions;
    auto& session_store = context.session_store;
    auto& security_metrics = context.security_metrics;
    const auto& config = context.config;
    gateway_router.on(
        apps::protocol::kReconnectRequest,
        [&sessions, &session_store, &security_metrics, &config](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::ReconnectRequest request;
            if (!runtime::protocol::unpack_message(envelope, request)) {
                security_metrics.record_reconnect_failed();
                return runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid reconnect request");
            }
            if (envelope.player_id() != request.player_id() ||
                envelope.session_token() != request.session_token() ||
                envelope.game_session_id() != request.game_session_id() ||
                request.context().player_id() != request.player_id() ||
                request.context().session_token() != request.session_token() ||
                request.context().game_session_id() != request.game_session_id()) {
                security_metrics.record_reconnect_failed();
                return runtime::protocol::make_error_envelope(
                    envelope, 400, "reconnect context does not match envelope");
            }

            const auto now_millis = static_cast<std::uint64_t>(
                runtime::protocol::current_time_millis());
            runtime::protocol::AuthTokenClaims claims;
            std::string token_error;
            if (!runtime::protocol::validate_auth_token(
                    request.reconnect_ticket(),
                    runtime::protocol::AuthTokenPurpose::kReconnect,
                    config.security.gateway_ticket.gateway_audience,
                    runtime::gateway::make_gateway_token_options(config.security.gateway_ticket),
                    static_cast<std::int64_t>(now_millis),
                    &claims,
                    &token_error)) {
                security_metrics.record_reconnect_failed();
                return runtime::protocol::make_error_envelope(
                    envelope, 401, "reconnect ticket authentication failed");
            }
            if (claims.player_id != request.player_id() ||
                claims.session_token != request.session_token()) {
                security_metrics.record_reconnect_failed();
                return runtime::protocol::make_error_envelope(
                    envelope, 400, "reconnect ticket does not match request");
            }

            runtime::session::ReconnectTicket consumed_ticket;
            std::string session_store_error;
            if (!session_store.consume_reconnect_ticket(
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
                return runtime::protocol::make_error_envelope(
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
            if (!session_store.save_binding(binding, &session_store_error)) {
                security_metrics.record_reconnect_failed();
                return runtime::protocol::make_error_envelope(
                    envelope, 503, "gateway session store is unavailable");
            }

            std::string next_reconnect_ticket;
            std::int64_t next_reconnect_ticket_expires_at = 0;
            if (!runtime::gateway::issue_reconnect_ticket(
                    config,
                    session_store,
                    binding,
                    now_millis,
                    &next_reconnect_ticket,
                    &next_reconnect_ticket_expires_at,
                    &session_store_error)) {
                security_metrics.record_reconnect_failed();
                return runtime::protocol::make_error_envelope(
                    envelope, 503, "reconnect ticket issue failed");
            }
            security_metrics.record_reconnect_success();

            mmo::public_api::ReconnectResponse response;
            auto response_context = request.context();
            response_context.set_game_session_id(binding.game_session_id);
            *response.mutable_context() =
                runtime::protocol::make_ok_context(response_context);
            response.set_reconnected(true);
            response.set_connection_id(binding.connection_id);
            response.set_game_session_id(binding.game_session_id);
            response.set_expires_at_epoch_millis(
                static_cast<std::int64_t>(binding.expire_at_millis));
            response.set_reconnect_ticket(next_reconnect_ticket);
            response.set_reconnect_ticket_expires_at_epoch_millis(
                next_reconnect_ticket_expires_at);
            return runtime::protocol::pack_message(
                apps::protocol::kReconnectResponse,
                response_context,
                response);
        });
}

}  // namespace apps::game_gateway_server

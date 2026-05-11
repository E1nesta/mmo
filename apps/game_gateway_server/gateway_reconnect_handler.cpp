#include "apps/game_gateway_server/gateway_reconnect_handler.h"

#include <cstdint>
#include <string>

#include "apps/game_gateway_server/gateway_frame_utils.h"
#include "cs/gateway.pb.h"
#include "proto/message_catalog.h"
#include "runtime/gateway/gateway_middleware.h"
#include "runtime/protocol/auth_tokens.h"
#include "runtime/protocol/payload_utils.h"

namespace apps::game_gateway_server {

void register_gateway_reconnect_handler(
    runtime::gateway::GatewayRouter& gateway_router,
    runtime::gateway::GatewaySessionContext& context) {
    auto& sessions = context.sessions;
    auto& session_store = context.session_store;
    auto& security_metrics = context.security_metrics;
    const auto options = context.options;
    gateway_router.on_sync(
        static_cast<std::uint16_t>(mmo::protocol::kReconnectRequest),
        [&sessions, &session_store, &security_metrics, options](
            const runtime::net::ReliableFrame& frame) {
            mmo::cs::ReconnectRequest request;
            if (!parse_gateway_payload(frame, &request)) {
                security_metrics.record_reconnect_failed();
                return runtime::gateway::make_gateway_error_frame(
                    frame, 400, "invalid reconnect request");
            }
            if (request.player_id() <= 0 ||
                request.session_token().empty() ||
                request.game_session_id().empty()) {
                security_metrics.record_reconnect_failed();
                return runtime::gateway::make_gateway_error_frame(
                    frame, 400, "invalid reconnect identity");
            }

            const auto now_millis = static_cast<std::uint64_t>(
                runtime::protocol::current_time_millis());
            runtime::protocol::AuthTokenClaims claims;
            std::string token_error;
            if (!runtime::protocol::validate_auth_token(
                    request.reconnect_ticket(),
                    runtime::protocol::AuthTokenPurpose::kReconnect,
                    options.gateway_audience,
                    options.token_options,
                    static_cast<std::int64_t>(now_millis),
                    &claims,
                    &token_error)) {
                security_metrics.record_reconnect_failed();
                return runtime::gateway::make_gateway_error_frame(
                    frame, 401, "reconnect ticket authentication failed");
            }
            if (claims.player_id != request.player_id() ||
                claims.session_token != request.session_token()) {
                security_metrics.record_reconnect_failed();
                return runtime::gateway::make_gateway_error_frame(
                    frame, 400, "reconnect ticket does not match request");
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
                consumed_ticket.gateway_id != options.gateway_id) {
                security_metrics.record_reconnect_failed();
                return runtime::gateway::make_gateway_error_frame(
                    frame, 401, "reconnect ticket authentication failed");
            }

            const auto expires_at =
                now_millis +
                static_cast<std::uint64_t>(
                    options.game_session_ttl_millis);
            const auto binding = sessions.reconnect(
                consumed_ticket.account_id,
                request.player_id(),
                request.session_token(),
                request.game_session_id(),
                options.gateway_id,
                request.device_id().empty()
                    ? consumed_ticket.device_id
                    : request.device_id(),
                now_millis,
                expires_at,
                static_cast<std::uint64_t>(
                    options.heartbeat_timeout_millis));
            if (!session_store.save_binding(binding, &session_store_error)) {
                security_metrics.record_reconnect_failed();
                return runtime::gateway::make_gateway_error_frame(
                    frame, 503, "gateway session store is unavailable");
            }

            std::string next_reconnect_ticket;
            std::int64_t next_reconnect_ticket_expires_at = 0;
            if (!runtime::gateway::issue_reconnect_ticket(
                    options,
                    session_store,
                    binding,
                    now_millis,
                    &next_reconnect_ticket,
                    &next_reconnect_ticket_expires_at,
                    &session_store_error)) {
                security_metrics.record_reconnect_failed();
                return runtime::gateway::make_gateway_error_frame(
                    frame, 503, "reconnect ticket issue failed");
            }
            security_metrics.record_reconnect_success();

            mmo::cs::ReconnectResponse response;
            *response.mutable_result() = runtime::protocol::make_ok_result();
            response.set_reconnected(true);
            response.set_connection_id(binding.connection_id);
            response.set_game_session_id(binding.game_session_id);
            response.set_expires_at_epoch_millis(
                static_cast<std::int64_t>(binding.expire_at_millis));
            response.set_reconnect_ticket(next_reconnect_ticket);
            response.set_reconnect_ticket_expires_at_epoch_millis(
                next_reconnect_ticket_expires_at);
            auto response_frame = make_gateway_payload_frame(
                frame,
                static_cast<std::uint16_t>(mmo::protocol::kReconnectResponse),
                response);
            response_frame.session_id = binding.connection_id;
            return response_frame;
        });
}

}  // namespace apps::game_gateway_server

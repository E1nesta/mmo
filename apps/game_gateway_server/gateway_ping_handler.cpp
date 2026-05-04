#include "apps/game_gateway_server/gateway_ping_handler.h"

#include <cstdint>
#include <string>

#include "runtime/gateway/gateway_middleware.h"
#include "public/gateway.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/internal_auth.h"
#include "apps/protocol/message_types.h"

namespace apps::game_gateway_server {

void register_gateway_ping_handler(
    runtime::gateway::GatewayRouter& gateway_router,
    runtime::gateway::GatewaySessionContext& context) {
    auto& sessions = context.sessions;
    auto& session_store = context.session_store;
    auto& security_metrics = context.security_metrics;
    const auto& config = context.config;
    gateway_router.on(
        apps::protocol::kPingRequest,
        [&sessions, &session_store, &security_metrics, &config](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::PingRequest request;
            if (!runtime::protocol::unpack_message(envelope, request)) {
                return runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid ping request");
            }
            const auto validation_error =
                runtime::gateway::validate_gateway_session(
                    envelope,
                    sessions,
                    session_store,
                    request.context(),
                    &security_metrics);
            if (validation_error.has_value()) {
                return *validation_error;
            }
            const auto now_millis = static_cast<std::uint64_t>(
                runtime::protocol::current_time_millis());
            if (!sessions.touch(
                    envelope.player_id(), envelope.game_session_id(), now_millis)) {
                security_metrics.record_game_session_expired();
                return runtime::protocol::make_error_envelope(
                    envelope, 401, "game session is not bound to gateway");
            }
            std::string session_store_error;
            if (!session_store.touch_binding(
                    envelope.player_id(),
                    envelope.game_session_id(),
                    now_millis,
                    &session_store_error)) {
                security_metrics.record_game_session_expired();
                return runtime::protocol::make_error_envelope(
                    envelope, 401, "game session is not bound to gateway");
            }
            const auto binding = sessions.find(envelope.player_id());
            if (!binding.has_value()) {
                security_metrics.record_game_session_expired();
                return runtime::protocol::make_error_envelope(
                    envelope, 401, "game session is not bound to gateway");
            }
            std::string reconnect_ticket;
            std::int64_t reconnect_ticket_expires_at = 0;
            if (!runtime::gateway::issue_reconnect_ticket(
                    config,
                    session_store,
                    *binding,
                    now_millis,
                    &reconnect_ticket,
                    &reconnect_ticket_expires_at,
                    &session_store_error)) {
                return runtime::protocol::make_error_envelope(
                    envelope, 503, "reconnect ticket issue failed");
            }

            mmo::public_api::PingResponse response;
            *response.mutable_context() =
                runtime::protocol::make_ok_context(request.context());
            response.set_server_time_ms(static_cast<std::int64_t>(now_millis));
            response.set_reconnect_ticket(reconnect_ticket);
            response.set_reconnect_ticket_expires_at_epoch_millis(
                reconnect_ticket_expires_at);
            return runtime::protocol::pack_message(
                apps::protocol::kPingResponse,
                request.context(),
                response);
        });
}

}  // namespace apps::game_gateway_server

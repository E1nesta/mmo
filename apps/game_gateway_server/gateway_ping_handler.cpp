#include "apps/game_gateway_server/gateway_ping_handler.h"

#include <cstdint>
#include <string>

#include "apps/game_gateway_server/gateway_frame_utils.h"
#include "cs/gateway.pb.h"
#include "proto/message_catalog.h"
#include "runtime/gateway/gateway_middleware.h"
#include "runtime/protocol/payload_utils.h"

namespace apps::game_gateway_server {

void register_gateway_ping_handler(
    runtime::gateway::GatewayRouter& gateway_router,
    runtime::gateway::GatewaySessionContext& context) {
    auto& sessions = context.sessions;
    auto& session_store = context.session_store;
    auto& security_metrics = context.security_metrics;
    const auto options = context.options;
    gateway_router.on_sync(
        static_cast<std::uint16_t>(mmo::protocol::kPingRequest),
        [&sessions, &session_store, &security_metrics, options](
            const runtime::net::ReliableFrame& frame) {
            mmo::cs::PingRequest request;
            if (!parse_gateway_payload(frame, &request)) {
                return runtime::gateway::make_gateway_error_frame(
                    frame, 400, "invalid ping request");
            }
            const auto validation_error =
                runtime::gateway::validate_gateway_session(
                    frame,
                    sessions,
                    &security_metrics);
            if (validation_error.has_value()) {
                return *validation_error;
            }
            const auto now_millis = static_cast<std::uint64_t>(
                runtime::protocol::current_time_millis());
            const auto binding = sessions.find_by_connection_id(frame.session_id);
            if (!binding.has_value() ||
                !sessions.touch(
                    binding->player_id, binding->game_session_id, now_millis)) {
                security_metrics.record_game_session_expired();
                return runtime::gateway::make_gateway_error_frame(
                    frame, 401, "game session is not bound to gateway");
            }
            std::string session_store_error;
            if (!session_store.touch_binding(
                    binding->player_id,
                    binding->game_session_id,
                    now_millis,
                    &session_store_error)) {
                security_metrics.record_game_session_expired();
                return runtime::gateway::make_gateway_error_frame(
                    frame, 401, "game session is not bound to gateway");
            }
            std::string reconnect_ticket;
            std::int64_t reconnect_ticket_expires_at = 0;
            if (!runtime::gateway::issue_reconnect_ticket(
                    options,
                    session_store,
                    *binding,
                    now_millis,
                    &reconnect_ticket,
                    &reconnect_ticket_expires_at,
                    &session_store_error)) {
                return runtime::gateway::make_gateway_error_frame(
                    frame, 503, "reconnect ticket issue failed");
            }

            mmo::cs::PingResponse response;
            *response.mutable_result() = runtime::protocol::make_ok_result();
            response.set_server_time_ms(static_cast<std::int64_t>(now_millis));
            response.set_reconnect_ticket(reconnect_ticket);
            response.set_reconnect_ticket_expires_at_epoch_millis(
                reconnect_ticket_expires_at);
            return make_gateway_payload_frame(
                frame,
                static_cast<std::uint16_t>(mmo::protocol::kPingResponse),
                response);
        });
}

}  // namespace apps::game_gateway_server

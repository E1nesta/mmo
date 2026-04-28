#include "internal/gateway_world.pb.h"
#include "modules/gateway/gateway_session.h"
#include "public/gateway.pb.h"
#include "public/world.pb.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_router.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_client.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::runtime::foundation::ServerApp app("gateway_server");
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(app.config().transport.tcp);
    mmo::runtime::transport::TcpEnvelopeClient upstream_client(tcp_options);

    mmo::modules::gateway::GatewaySessionRegistry sessions;
    mmo::runtime::protocol::MessageRouter router;

    router.on(
        mmo::runtime::protocol::kGateLoginRequest,
        [&sessions](const mmo::common::Envelope& envelope) {
            mmo::public_api::GateLoginRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid gate login request");
            }

            const auto binding =
                sessions.bind(request.player_id(), request.session_token());

            mmo::public_api::GateLoginResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_bound(true);
            response.set_connection_id(binding.connection_id);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGateLoginResponse,
                request.context(),
                response);
        });

    router.on(
        mmo::runtime::protocol::kEnterWorldRequest,
        [&sessions, &app, &upstream_client](const mmo::common::Envelope& envelope) {
            mmo::public_api::EnterWorldRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid enter world request");
            }
            if (!sessions.is_bound(
                    request.context().player_id(),
                    request.context().session_token())) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "player is not bound to gateway");
            }

            mmo::internal_api::GatewayEnterWorldRequest internal_request;
            *internal_request.mutable_context() = request.context();
            internal_request.set_preferred_map_id(request.preferred_map_id());
            internal_request.set_preferred_line_id(request.preferred_line_id());

            const auto internal_envelope = mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGatewayEnterWorldRequest,
                request.context(),
                internal_request);
            const auto internal_response_envelope = upstream_client.send(
                mmo::runtime::transport::make_transport_endpoint(
                    app.service_config("world_server")),
                internal_envelope);

            mmo::internal_api::GatewayEnterWorldResponse internal_response;
            if (!mmo::runtime::protocol::unpack_message(
                    internal_response_envelope, internal_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, "invalid world response");
            }
            if (!internal_response.context().success()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    internal_response.context().error_code(),
                    internal_response.context().error_message());
            }

            mmo::public_api::EnterWorldResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            *response.mutable_route() = internal_response.route();
            response.set_scene_entity_id(internal_response.scene_entity_id());
            *response.mutable_spawn_position() = internal_response.spawn_position();

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kEnterWorldResponse,
                request.context(),
                response);
        });

    mmo::runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        router.handler(),
        app.service_name(),
        tcp_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

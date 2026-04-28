#include "internal/gateway_world.pb.h"
#include "internal/world_scene.pb.h"
#include "modules/world/world_service.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_router.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_client.h"
#include "runtime/transport/tcp_envelope_server.h"

namespace {

mmo::common::SceneRoute to_proto(const mmo::modules::world::SceneRoute& route) {
    mmo::common::SceneRoute proto;
    proto.set_map_id(route.map_id);
    proto.set_line_id(route.line_id);
    proto.set_scene_id(route.scene_id);
    return proto;
}

}  // namespace

int main() {
    mmo::runtime::foundation::ServerApp app("world_server");
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(app.config().transport.tcp);
    mmo::runtime::transport::TcpEnvelopeClient upstream_client(tcp_options);

    mmo::modules::world::WorldService service;
    mmo::runtime::protocol::MessageRouter router;

    router.on(
        mmo::runtime::protocol::kGatewayEnterWorldRequest,
        [&service, &app, &upstream_client](const mmo::common::Envelope& envelope) {
            mmo::internal_api::GatewayEnterWorldRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid gateway enter world request");
            }

            const auto route = service.enter_world(
                request.context().player_id(),
                request.preferred_map_id(),
                request.preferred_line_id());

            mmo::internal_api::AllocateSceneEntityRequest scene_request;
            *scene_request.mutable_context() = request.context();
            *scene_request.mutable_route() = to_proto(route);

            const auto scene_envelope = mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kAllocateSceneEntityRequest,
                request.context(),
                scene_request);
            const auto scene_response_envelope = upstream_client.send(
                mmo::runtime::transport::make_transport_endpoint(
                    app.service_config("scene_server")),
                scene_envelope);

            mmo::internal_api::AllocateSceneEntityResponse scene_response;
            if (!mmo::runtime::protocol::unpack_message(
                    scene_response_envelope, scene_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, "invalid scene response");
            }
            if (!scene_response.context().success()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    scene_response.context().error_code(),
                    scene_response.context().error_message());
            }

            mmo::internal_api::GatewayEnterWorldResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            *response.mutable_route() = to_proto(route);
            response.set_scene_entity_id(scene_response.entity_id());
            *response.mutable_spawn_position() = scene_response.position();

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGatewayEnterWorldResponse,
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

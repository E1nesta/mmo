#include <iostream>

#include "modules/world/world_service.h"
#include "public/scene.pb.h"
#include "public/world.pb.h"
#include "runtime/foundation/service_ports.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/tcp_envelope_server.h"

namespace {

mmo::public_api::SceneRoute to_proto(const mmo::modules::world::SceneRoute& route) {
    mmo::public_api::SceneRoute proto;
    proto.set_map_id(route.map_id);
    proto.set_line_id(route.line_id);
    proto.set_scene_id(route.scene_id);
    return proto;
}

}  // namespace

int main() {
    mmo::modules::world::WorldService service;

    mmo::runtime::transport::TcpEnvelopeServer server(
        mmo::runtime::foundation::kWorldServerPort,
        [&service](const mmo::public_api::Envelope& envelope) {
            if (envelope.message_type() != mmo::runtime::protocol::kEnterWorldRequest) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "unsupported world message");
            }

            mmo::public_api::EnterWorldRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid enter world request");
            }

            const auto route = service.enter_world(
                request.context().player_id(),
                request.preferred_map_id(),
                request.preferred_line_id());

            mmo::public_api::EnterSceneRequest scene_request;
            *scene_request.mutable_context() = request.context();
            *scene_request.mutable_route() = to_proto(route);

            const auto scene_envelope = mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kEnterSceneRequest,
                request.context(),
                scene_request);
            const auto scene_response_envelope =
                mmo::runtime::transport::send_envelope(
                    mmo::runtime::foundation::kLocalhost,
                    mmo::runtime::foundation::kSceneServerPort,
                    scene_envelope);

            mmo::public_api::EnterSceneResponse scene_response;
            if (!mmo::runtime::protocol::unpack_message(
                    scene_response_envelope, scene_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, "invalid scene response");
            }

            mmo::public_api::EnterWorldResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            *response.mutable_route() = to_proto(route);
            response.set_scene_entity_id(scene_response.entity_id());
            *response.mutable_spawn_position() = scene_response.position();

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kEnterWorldResponse,
                request.context(),
                response);
        });

    std::cout << "world_server starting\n";
    return server.run();
}

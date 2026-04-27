#include <iostream>

#include "modules/scene/scene_service.h"
#include "public/scene.pb.h"
#include "runtime/foundation/service_ports.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::modules::scene::SceneService service;

    mmo::runtime::transport::TcpEnvelopeServer server(
        mmo::runtime::foundation::kSceneServerPort,
        [&service](const mmo::public_api::Envelope& envelope) {
            if (envelope.message_type() != mmo::runtime::protocol::kEnterSceneRequest) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "unsupported scene message");
            }

            mmo::public_api::EnterSceneRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid enter scene request");
            }

            const auto entity = service.enter_scene(
                request.context().player_id(),
                request.route().scene_id());

            mmo::public_api::EnterSceneResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_entity_id(entity.entity_id);
            response.mutable_position()->set_x(entity.transform.x);
            response.mutable_position()->set_y(entity.transform.y);
            response.mutable_position()->set_z(entity.transform.z);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kEnterSceneResponse,
                request.context(),
                response);
        });

    std::cout << "scene_server starting\n";
    return server.run();
}

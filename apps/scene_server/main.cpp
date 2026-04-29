#include "internal/world_scene.pb.h"
#include "modules/scene/scene_service.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::runtime::foundation::ServerApp app("scene_server");
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(
            app.config().transport.tcp, app.config().execution);

    mmo::modules::scene::SceneService service;
    mmo::runtime::rpc::RpcServer rpc_server(
        mmo::runtime::rpc::make_rpc_server_options(app.config()));

    rpc_server.on(
        mmo::runtime::protocol::kAllocateSceneEntityRequest,
        [&service](const mmo::common::Envelope& envelope) {
            mmo::internal_api::AllocateSceneEntityRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid allocate scene entity request");
            }

            const auto entity = service.enter_scene(
                request.context().player_id(),
                request.route().scene_id());

            mmo::internal_api::AllocateSceneEntityResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_entity_id(entity.entity_id);
            response.mutable_position()->set_x(entity.transform.x);
            response.mutable_position()->set_y(entity.transform.y);
            response.mutable_position()->set_z(entity.transform.z);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kAllocateSceneEntityResponse,
                request.context(),
                response);
        });

    mmo::runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        rpc_server.handler(),
        app.service_name(),
        tcp_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

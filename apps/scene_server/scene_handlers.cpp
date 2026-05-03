#include "apps/scene_server/scene_handlers.h"

#include "internal/world_scene.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"

namespace mmo::apps::scene_server {

void register_scene_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::scene::SceneService& service) {
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
}

}  // namespace mmo::apps::scene_server

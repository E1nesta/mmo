#include "apps/scene_server/scene_handlers.h"

#include <utility>

#include "internal/world_scene.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "apps/protocol/message_types.h"
#include "runtime/server/handler_result.h"
#include "runtime/server/typed_handler.h"

namespace mmo::apps::scene_server {

void register_scene_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::scene::SceneService& service) {
    mmo::runtime::server::bind_typed_handler<
        mmo::internal_api::AllocateSceneEntityRequest,
        mmo::internal_api::AllocateSceneEntityResponse>(
        rpc_server,
        mmo::apps::protocol::kAllocateSceneEntityRequest,
        mmo::apps::protocol::kAllocateSceneEntityResponse,
        "scene_server",
        [&service](
            const mmo::internal_api::AllocateSceneEntityRequest& request,
            const mmo::runtime::server::ServiceContext&) {
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

            return mmo::runtime::server::HandlerResult<
                mmo::internal_api::AllocateSceneEntityResponse>::success(
                    std::move(response));
        });
}

}  // namespace mmo::apps::scene_server

#include "apps/scene_server/scene_handlers.h"

#include <utility>

#include "internal/world_scene.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "apps/protocol/message_types.h"
#include "runtime/server/handler_result.h"
#include "runtime/server/typed_handler.h"

namespace apps::scene_server {

namespace app_proto = apps::protocol;
namespace protocol = runtime::protocol;
namespace rpc = runtime::rpc;
namespace server = runtime::server;
namespace scene = modules::scene;

void register_scene_handlers(
    rpc::RpcServer& rpc_server,
    scene::SceneService& service) {
    server::bind_typed_handler<
        mmo::internal_api::AllocateSceneEntityRequest,
        mmo::internal_api::AllocateSceneEntityResponse>(
        rpc_server,
        app_proto::kAllocateSceneEntityRequest,
        app_proto::kAllocateSceneEntityResponse,
        "scene_server",
        [&service](
            const mmo::internal_api::AllocateSceneEntityRequest& request,
            const server::ServiceContext&) {
            const auto entity = service.enter_scene(
                request.context().player_id(),
                request.route().scene_id());

            mmo::internal_api::AllocateSceneEntityResponse response;
            *response.mutable_context() =
                protocol::make_ok_context(request.context());
            response.set_entity_id(entity.entity_id);
            response.mutable_position()->set_x(entity.transform.x);
            response.mutable_position()->set_y(entity.transform.y);
            response.mutable_position()->set_z(entity.transform.z);

            return server::HandlerResult<
                mmo::internal_api::AllocateSceneEntityResponse>::success(
                    std::move(response));
        });
}

}  // namespace apps::scene_server

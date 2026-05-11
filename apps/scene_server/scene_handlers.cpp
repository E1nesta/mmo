#include "apps/scene_server/scene_handlers.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "ss/world_scene.pb.h"
#include "runtime/entity/entity_id.h"
#include "runtime/entity/entity_mailbox.h"
#include "runtime/entity/entity_state_store.h"
#include "proto/message_catalog.h"
#include "runtime/handler/handler_result.h"
#include "runtime/handler/typed_handler.h"
#include "runtime/protocol/payload_utils.h"

namespace apps::scene_server {

namespace app_proto = mmo::protocol;
namespace protocol = runtime::protocol;
namespace rpc = runtime::rpc;
namespace handler = runtime::handler;
namespace scene = modules::scene;

void register_scene_handlers(
    rpc::RpcDispatcher& dispatcher,
    scene::SceneService& service,
    runtime::entity::EntityExecutor& entity_executor,
    runtime::scheduler::ShardedExecutor& entity_scheduler) {
    using SceneStateStore =
        runtime::entity::EntityStateStore<std::int64_t, scene::SceneState>;
    auto scene_states = std::make_shared<SceneStateStore>();

    handler::bind_async_typed_handler<
        mmo::ss::AllocateSceneEntityRequest,
        mmo::ss::AllocateSceneEntityResponse>(
        dispatcher,
        app_proto::kAllocateSceneEntityRequest,
        app_proto::kAllocateSceneEntityResponse,
        "scene_server",
        [&service,
         &entity_executor,
         &entity_scheduler,
         scene_states](
            const mmo::ss::AllocateSceneEntityRequest& request,
            const handler::HandlerContext& context,
            auto reply) {
            const auto player_id = static_cast<std::int64_t>(context.route_key);
            if (player_id <= 0) {
                reply(handler::HandlerResult<
                    mmo::ss::AllocateSceneEntityResponse>::failure(
                        401, "player route key is required"));
                return;
            }
            if (request.route().scene_id() <= 0) {
                reply(handler::HandlerResult<
                    mmo::ss::AllocateSceneEntityResponse>::failure(
                        400, "scene route is invalid"));
                return;
            }

            const auto entity_id =
                runtime::entity::scene_entity(request.route().scene_id());
            auto reply_once = std::make_shared<decltype(reply)>(std::move(reply));
            auto scene_state =
                scene_states->find_or_create(request.route().scene_id());
            auto drain = entity_executor.submit(
                entity_scheduler,
                runtime::entity::EntityMessage{
                    entity_id,
                    context.message_id,
                    context.request_id,
                    request.route().scene_id()},
                [&service,
                 request,
                 player_id,
                 scene_state,
                 reply_once](
                    const runtime::entity::EntityMessage&) mutable {
                    const auto entity = service.enter_scene(
                        *scene_state,
                        player_id,
                        request.route().scene_id());

                    mmo::ss::AllocateSceneEntityResponse response;
                    *response.mutable_result() = protocol::make_ok_result();
                    response.set_entity_id(entity.entity_id);
                    response.mutable_position()->set_x(entity.transform.x);
                    response.mutable_position()->set_y(entity.transform.y);
                    response.mutable_position()->set_z(entity.transform.z);
                    (*reply_once)(handler::HandlerResult<
                        mmo::ss::AllocateSceneEntityResponse>::success(
                            std::move(response)));
                });
            if (!drain.accepted()) {
                (*reply_once)(handler::HandlerResult<
                    mmo::ss::AllocateSceneEntityResponse>::failure(
                        503, "scene entity executor is unavailable"));
                return;
            }
        });
}

}  // namespace apps::scene_server

#include "apps/world_server/world_handlers.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "common/types.pb.h"
#include "cs/world.pb.h"
#include "ss/world_scene.pb.h"
#include "runtime/entity/entity_id.h"
#include "runtime/entity/entity_mailbox.h"
#include "runtime/rpc/rpc_routing_policy.h"
#include "proto/message_catalog.h"
#include "runtime/rpc/rpc_error.h"
#include "runtime/rpc/rpc_options.h"
#include "runtime/rpc/rpc_result.h"
#include "runtime/handler/handler_result.h"
#include "runtime/handler/typed_handler.h"
#include "runtime/protocol/payload_utils.h"

namespace apps::world_server {

namespace app_proto = mmo::protocol;
namespace rpc = runtime::rpc;
namespace protocol = runtime::protocol;
namespace handler = runtime::handler;
namespace world = modules::world;
namespace {

mmo::common::SceneRoute to_proto(const world::SceneRoute& route) {
    mmo::common::SceneRoute proto;
    proto.set_map_id(route.map_id);
    proto.set_line_id(route.line_id);
    proto.set_scene_id(route.scene_id);
    return proto;
}

}  // namespace

void register_world_handlers(
    rpc::RpcDispatcher& dispatcher,
    world::WorldService& service,
    rpc::RpcClient& rpc_client,
    runtime::entity::EntityExecutor& entity_executor,
    runtime::scheduler::ShardedExecutor& entity_scheduler,
    const std::string& service_name) {
    auto world_state = std::make_shared<world::WorldState>();

    handler::bind_async_typed_handler<
        mmo::cs::EnterWorldRequest,
        mmo::cs::EnterWorldResponse>(
        dispatcher,
        app_proto::kEnterWorldRequest,
        app_proto::kEnterWorldResponse,
        service_name,
        [&service,
         &rpc_client,
         &entity_executor,
         &entity_scheduler,
         world_state](
            const mmo::cs::EnterWorldRequest& request,
            const handler::HandlerContext& context,
            auto reply) {
            const auto player_id = static_cast<std::int64_t>(context.route_key);
            if (player_id <= 0) {
                reply(handler::HandlerResult<
                    mmo::cs::EnterWorldResponse>::failure(
                        401, "player route key is required"));
                return;
            }

            constexpr std::uint64_t kDefaultWorldEntityId = 1;
            const auto entity_id =
                runtime::entity::world_entity(kDefaultWorldEntityId);
            auto reply_once = std::make_shared<decltype(reply)>(std::move(reply));
            auto drain = entity_executor.submit(
                entity_scheduler,
                runtime::entity::EntityMessage{
                    entity_id,
                    context.message_id,
                    context.request_id,
                    kDefaultWorldEntityId},
                [&service,
                 &rpc_client,
                 request,
                 context,
                 player_id,
                 world_state,
                 reply_once](
                    const runtime::entity::EntityMessage&) mutable {
                    const auto route = service.enter_world(
                        *world_state,
                        player_id,
                        request.preferred_map_id(),
                        request.preferred_line_id());

                    mmo::ss::AllocateSceneEntityRequest scene_request;
                    *scene_request.mutable_route() = to_proto(route);

                    rpc::RpcOptions controller;
                    controller.source_service = context.service_name;
                    controller.routing_policy =
                        rpc::RpcRoutingPolicy::kLeastPending;
                    const auto rpc_error = rpc_client.call_async(
                        "scene_server",
                        app_proto::kAllocateSceneEntityRequest,
                        static_cast<std::uint64_t>(player_id),
                        scene_request,
                        std::move(controller),
                        [route, reply_once](
                            rpc::RpcResult rpc_result) mutable {
                            if (!rpc_result.ok()) {
                                (*reply_once)(handler::HandlerResult<
                                    mmo::cs::EnterWorldResponse>::failure(
                                        rpc::rpc_error_to_status_code(
                                            rpc_result.error().code),
                                        rpc_result.error().message.empty()
                                            ? "scene rpc failed"
                                            : rpc_result.error().message));
                                return;
                            }

                            mmo::ss::AllocateSceneEntityResponse scene_response;
                            if (!protocol::parse_payload(
                                    rpc_result.response(), &scene_response)) {
                                (*reply_once)(handler::HandlerResult<
                                    mmo::cs::EnterWorldResponse>::failure(
                                        502, "invalid scene response"));
                                return;
                            }
                            if (!scene_response.result().ok()) {
                                (*reply_once)(handler::HandlerResult<
                                    mmo::cs::EnterWorldResponse>::failure(
                                        scene_response.result().error().code(),
                                        scene_response.result().error().message()));
                                return;
                            }

                            mmo::cs::EnterWorldResponse response;
                            *response.mutable_result() = protocol::make_ok_result();
                            *response.mutable_route() = to_proto(route);
                            response.set_scene_entity_id(
                                scene_response.entity_id());
                            *response.mutable_spawn_position() =
                                scene_response.position();

                            (*reply_once)(handler::HandlerResult<
                                mmo::cs::EnterWorldResponse>::success(
                                    std::move(response)));
                        });
                    if (!rpc_error.ok()) {
                        (*reply_once)(handler::HandlerResult<
                            mmo::cs::EnterWorldResponse>::failure(
                                rpc::rpc_error_to_status_code(rpc_error.code),
                                rpc_error.message.empty()
                                    ? "scene rpc failed"
                                    : rpc_error.message));
                    }
                });
            if (!drain.accepted()) {
                (*reply_once)(handler::HandlerResult<
                    mmo::cs::EnterWorldResponse>::failure(
                        503, "world entity executor is unavailable"));
                return;
            }
        });
}

}  // namespace apps::world_server

#include "apps/world_server/world_handlers.h"

#include <utility>

#include "common/types.pb.h"
#include "internal/gateway_world.pb.h"
#include "internal/world_scene.pb.h"
#include "runtime/channel/routing_policy.h"
#include "runtime/protocol/envelope_utils.h"
#include "apps/protocol/message_types.h"
#include "runtime/rpc/rpc_controller.h"
#include "runtime/rpc/rpc_error.h"
#include "runtime/rpc/rpc_result.h"
#include "runtime/server/handler_result.h"
#include "runtime/server/typed_handler.h"

namespace apps::world_server {

namespace app_proto = apps::protocol;
namespace channel = runtime::channel;
namespace protocol = runtime::protocol;
namespace rpc = runtime::rpc;
namespace server = runtime::server;
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
    rpc::RpcServer& rpc_server,
    world::WorldService& service,
    rpc::RpcClient& rpc_client,
    const std::string& service_name) {
    server::bind_typed_handler<
        mmo::internal_api::GatewayEnterWorldRequest,
        mmo::internal_api::GatewayEnterWorldResponse>(
        rpc_server,
        app_proto::kGatewayEnterWorldRequest,
        app_proto::kGatewayEnterWorldResponse,
        service_name,
        [&service, &rpc_client](
            const mmo::internal_api::GatewayEnterWorldRequest& request,
            const server::ServiceContext& context) {
            const auto route = service.enter_world(
                request.context().player_id(),
                request.preferred_map_id(),
                request.preferred_line_id());

            mmo::internal_api::AllocateSceneEntityRequest scene_request;
            *scene_request.mutable_context() = request.context();
            *scene_request.mutable_route() = to_proto(route);

            rpc::RpcController controller;
            controller.source_service = context.service_name;
            controller.routing_policy =
                channel::RoutingPolicy::kLeastPending;
            const auto rpc_result = rpc_client.call(
                "scene_server",
                app_proto::kAllocateSceneEntityRequest,
                request.context(),
                scene_request,
                controller);
            if (!rpc_result.ok()) {
                return server::HandlerResult<
                    mmo::internal_api::GatewayEnterWorldResponse>::failure(
                        rpc::rpc_error_to_status_code(
                            rpc_result.error().code),
                        rpc_result.error().message.empty()
                            ? "scene rpc failed"
                            : rpc_result.error().message);
            }

            mmo::internal_api::AllocateSceneEntityResponse scene_response;
            if (!protocol::unpack_message(
                    rpc_result.response(), scene_response)) {
                return server::HandlerResult<
                    mmo::internal_api::GatewayEnterWorldResponse>::failure(
                        502, "invalid scene response");
            }
            if (!scene_response.context().success()) {
                return server::HandlerResult<
                    mmo::internal_api::GatewayEnterWorldResponse>::failure(
                        scene_response.context().error_code(),
                        scene_response.context().error_message());
            }

            mmo::internal_api::GatewayEnterWorldResponse response;
            *response.mutable_context() =
                protocol::make_ok_context(request.context());
            *response.mutable_route() = to_proto(route);
            response.set_scene_entity_id(scene_response.entity_id());
            *response.mutable_spawn_position() = scene_response.position();

            return server::HandlerResult<
                mmo::internal_api::GatewayEnterWorldResponse>::success(
                    std::move(response));
        });
}

}  // namespace apps::world_server

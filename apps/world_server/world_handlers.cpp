#include "apps/world_server/world_handlers.h"

#include "common/types.pb.h"
#include "internal/gateway_world.pb.h"
#include "internal/world_scene.pb.h"
#include "runtime/channel/routing_policy.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/rpc/rpc_controller.h"
#include "runtime/rpc/rpc_result.h"

namespace mmo::apps::world_server {
namespace {

mmo::common::SceneRoute to_proto(const mmo::modules::world::SceneRoute& route) {
    mmo::common::SceneRoute proto;
    proto.set_map_id(route.map_id);
    proto.set_line_id(route.line_id);
    proto.set_scene_id(route.scene_id);
    return proto;
}

}  // namespace

void register_world_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::world::WorldService& service,
    mmo::runtime::rpc::RpcClient& rpc_client,
    const std::string& service_name) {
    rpc_server.on(
        mmo::runtime::protocol::kGatewayEnterWorldRequest,
        [&service, &rpc_client, service_name](
            const mmo::common::Envelope& envelope) {
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

            mmo::runtime::rpc::RpcController controller;
            controller.source_service = service_name;
            controller.routing_policy =
                mmo::runtime::channel::RoutingPolicy::kLeastPending;
            const auto rpc_result = rpc_client.call(
                "scene_server",
                mmo::runtime::protocol::kAllocateSceneEntityRequest,
                request.context(),
                scene_request,
                controller);
            if (!rpc_result.ok()) {
                return rpc_result.make_error_envelope(envelope);
            }

            mmo::internal_api::AllocateSceneEntityResponse scene_response;
            if (!mmo::runtime::protocol::unpack_message(
                    rpc_result.response(), scene_response)) {
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
}

}  // namespace mmo::apps::world_server

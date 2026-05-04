#include "apps/game_gateway_server/gateway_world_proxy_handlers.h"

#include <utility>

#include "internal/gateway_world.pb.h"
#include "public/world.pb.h"
#include "runtime/gateway/proxy_handler.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"

namespace mmo::apps::game_gateway_server {

void register_gateway_world_proxy_handlers(
    mmo::runtime::gateway::GatewayRouter& gateway_router,
    mmo::runtime::gateway::ProxyContext& context) {
    mmo::runtime::gateway::ProxyMapper<
        mmo::public_api::EnterWorldRequest,
        mmo::internal_api::GatewayEnterWorldRequest,
        mmo::internal_api::GatewayEnterWorldResponse,
        mmo::public_api::EnterWorldResponse>
        mapper;
    mapper.public_response_message_type =
        mmo::runtime::protocol::kEnterWorldResponse;
    mapper.invalid_public_request_message = "invalid enter world request";
    mapper.invalid_internal_response_message = "invalid world response";
    mapper.map_request = [](const mmo::public_api::EnterWorldRequest& request) {
        mmo::internal_api::GatewayEnterWorldRequest internal_request;
        *internal_request.mutable_context() = request.context();
        internal_request.set_preferred_map_id(request.preferred_map_id());
        internal_request.set_preferred_line_id(request.preferred_line_id());
        return internal_request;
    };
    mapper.map_response = [](
                              const mmo::public_api::EnterWorldRequest& request,
                              const mmo::internal_api::GatewayEnterWorldResponse&
                                  internal_response) {
        mmo::public_api::EnterWorldResponse response;
        *response.mutable_context() =
            mmo::runtime::protocol::make_ok_context(request.context());
        *response.mutable_route() = internal_response.route();
        response.set_scene_entity_id(internal_response.scene_entity_id());
        *response.mutable_spawn_position() = internal_response.spawn_position();
        return response;
    };

    mmo::runtime::gateway::bind_proxy_handler<
        mmo::public_api::EnterWorldRequest,
        mmo::internal_api::GatewayEnterWorldRequest,
        mmo::internal_api::GatewayEnterWorldResponse,
        mmo::public_api::EnterWorldResponse>(
        gateway_router,
        mmo::runtime::protocol::kEnterWorldRequest,
        context,
        std::move(mapper));
}

}  // namespace mmo::apps::game_gateway_server

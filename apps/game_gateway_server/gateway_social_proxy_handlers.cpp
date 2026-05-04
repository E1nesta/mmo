#include "apps/game_gateway_server/gateway_social_proxy_handlers.h"

#include <utility>

#include "internal/gateway_social.pb.h"
#include "public/social.pb.h"
#include "runtime/gateway/proxy_handler.h"
#include "runtime/protocol/envelope_utils.h"
#include "apps/protocol/message_types.h"

namespace apps::game_gateway_server {

namespace app_proto = apps::protocol;
namespace gateway = runtime::gateway;
namespace protocol = runtime::protocol;

void register_gateway_social_proxy_handlers(
    gateway::GatewayRouter& gateway_router,
    gateway::ProxyContext& context) {
    gateway::ProxyMapper<
        mmo::public_api::SocialBoundaryRequest,
        mmo::internal_api::GatewaySocialBoundaryRequest,
        mmo::internal_api::GatewaySocialBoundaryResponse,
        mmo::public_api::SocialBoundaryResponse>
        mapper;
    mapper.public_response_message_type =
        app_proto::kSocialBoundaryResponse;
    mapper.invalid_public_request_message = "invalid social boundary request";
    mapper.invalid_internal_response_message = "invalid social response";
    mapper.map_request =
        [](const mmo::public_api::SocialBoundaryRequest& request) {
            mmo::internal_api::GatewaySocialBoundaryRequest internal_request;
            *internal_request.mutable_context() = request.context();
            internal_request.set_target_player_id(request.target_player_id());
            return internal_request;
        };
    mapper.map_response =
        [](
            const mmo::public_api::SocialBoundaryRequest& request,
            const mmo::internal_api::GatewaySocialBoundaryResponse&
                internal_response) {
            mmo::public_api::SocialBoundaryResponse response;
            *response.mutable_context() =
                protocol::make_ok_context(request.context());
            response.set_friend_boundary_available(
                internal_response.friend_boundary_available());
            response.set_chat_boundary_available(
                internal_response.chat_boundary_available());
            response.set_team_boundary_available(
                internal_response.team_boundary_available());
            return response;
        };
    gateway::bind_proxy_handler<
        mmo::public_api::SocialBoundaryRequest,
        mmo::internal_api::GatewaySocialBoundaryRequest,
        mmo::internal_api::GatewaySocialBoundaryResponse,
        mmo::public_api::SocialBoundaryResponse>(
        gateway_router,
        app_proto::kSocialBoundaryRequest,
        context,
        std::move(mapper));
}

}  // namespace apps::game_gateway_server

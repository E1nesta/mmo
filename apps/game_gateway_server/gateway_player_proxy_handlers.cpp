#include "apps/game_gateway_server/gateway_player_proxy_handlers.h"

#include <utility>

#include "internal/gateway_player.pb.h"
#include "public/player.pb.h"
#include "runtime/gateway/proxy_handler.h"
#include "runtime/protocol/envelope_utils.h"
#include "apps/protocol/message_types.h"

namespace apps::game_gateway_server {

namespace app_proto = apps::protocol;
namespace gateway = runtime::gateway;
namespace protocol = runtime::protocol;

void register_gateway_player_proxy_handlers(
    gateway::GatewayRouter& gateway_router,
    gateway::ProxyContext& context) {
    gateway::ProxyMapper<
        mmo::public_api::ApplyRewardRequest,
        mmo::internal_api::GatewayApplyRewardRequest,
        mmo::internal_api::GatewayApplyRewardResponse,
        mmo::public_api::ApplyRewardResponse>
        mapper;
    mapper.public_response_message_type =
        app_proto::kApplyRewardResponse;
    mapper.invalid_public_request_message = "invalid apply reward request";
    mapper.invalid_internal_response_message = "invalid player response";
    mapper.map_request = [](const mmo::public_api::ApplyRewardRequest& request) {
        mmo::internal_api::GatewayApplyRewardRequest internal_request;
        *internal_request.mutable_context() = request.context();
        internal_request.set_idempotency_key(request.idempotency_key());
        for (const auto& reward : request.rewards()) {
            *internal_request.add_rewards() = reward;
        }
        return internal_request;
    };
    mapper.map_response = [](
                              const mmo::public_api::ApplyRewardRequest& request,
                              const mmo::internal_api::GatewayApplyRewardResponse&
                                  internal_response) {
        mmo::public_api::ApplyRewardResponse response;
        *response.mutable_context() =
            protocol::make_ok_context(request.context());
        response.set_applied(internal_response.applied());
        response.set_gold(internal_response.gold());
        response.set_exp(internal_response.exp());
        return response;
    };
    gateway::bind_proxy_handler<
        mmo::public_api::ApplyRewardRequest,
        mmo::internal_api::GatewayApplyRewardRequest,
        mmo::internal_api::GatewayApplyRewardResponse,
        mmo::public_api::ApplyRewardResponse>(
        gateway_router,
        app_proto::kApplyRewardRequest,
        context,
        std::move(mapper));
}

}  // namespace apps::game_gateway_server

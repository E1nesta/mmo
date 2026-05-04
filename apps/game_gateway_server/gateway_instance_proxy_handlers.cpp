#include "apps/game_gateway_server/gateway_instance_proxy_handlers.h"

#include <string>
#include <utility>

#include "internal/gateway_instance.pb.h"
#include "public/instance.pb.h"
#include "runtime/gateway/proxy_handler.h"
#include "runtime/protocol/envelope_utils.h"
#include "apps/protocol/message_types.h"

namespace apps::game_gateway_server {

namespace app_proto = apps::protocol;
namespace gateway = runtime::gateway;
namespace protocol = runtime::protocol;

void register_gateway_instance_proxy_handlers(
    gateway::GatewayRouter& gateway_router,
    gateway::ProxyContext& context) {
    gateway::ProxyMapper<
        mmo::public_api::EnterInstanceRequest,
        mmo::internal_api::GatewayEnterInstanceRequest,
        mmo::internal_api::GatewayEnterInstanceResponse,
        mmo::public_api::EnterInstanceResponse>
        enter_mapper;
    enter_mapper.public_response_message_type =
        app_proto::kEnterInstanceResponse;
    enter_mapper.invalid_public_request_message =
        "invalid enter instance request";
    enter_mapper.invalid_internal_response_message =
        "invalid instance response";
    enter_mapper.map_request =
        [](const mmo::public_api::EnterInstanceRequest& request) {
            mmo::internal_api::GatewayEnterInstanceRequest internal_request;
            *internal_request.mutable_context() = request.context();
            internal_request.set_dungeon_id(request.dungeon_id());
            return internal_request;
        };
    enter_mapper.map_response =
        [](
            const mmo::public_api::EnterInstanceRequest& request,
            const mmo::internal_api::GatewayEnterInstanceResponse&
                internal_response) {
            mmo::public_api::EnterInstanceResponse response;
            *response.mutable_context() =
                protocol::make_ok_context(request.context());
            response.set_instance_id(internal_response.instance_id());
            response.set_boss_entity_id(internal_response.boss_entity_id());
            return response;
        };
    gateway::bind_proxy_handler<
        mmo::public_api::EnterInstanceRequest,
        mmo::internal_api::GatewayEnterInstanceRequest,
        mmo::internal_api::GatewayEnterInstanceResponse,
        mmo::public_api::EnterInstanceResponse>(
        gateway_router,
        app_proto::kEnterInstanceRequest,
        context,
        std::move(enter_mapper));

    gateway::ProxyMapper<
        mmo::public_api::SettleInstanceRequest,
        mmo::internal_api::GatewaySettleInstanceRequest,
        mmo::internal_api::GatewaySettleInstanceResponse,
        mmo::public_api::SettleInstanceResponse>
        settle_mapper;
    settle_mapper.public_response_message_type =
        app_proto::kSettleInstanceResponse;
    settle_mapper.invalid_public_request_message =
        "invalid settle instance request";
    settle_mapper.invalid_internal_response_message =
        "invalid instance response";
    settle_mapper.map_request =
        [](const mmo::public_api::SettleInstanceRequest& request) {
            mmo::internal_api::GatewaySettleInstanceRequest internal_request;
            *internal_request.mutable_context() = request.context();
            internal_request.set_instance_id(request.instance_id());
            internal_request.set_idempotency_key(request.idempotency_key());
            internal_request.set_win(request.win());
            return internal_request;
        };
    settle_mapper.route_key =
        [](const mmo::public_api::SettleInstanceRequest& request) {
            return std::to_string(request.instance_id());
        };
    settle_mapper.map_response =
        [](
            const mmo::public_api::SettleInstanceRequest& request,
            const mmo::internal_api::GatewaySettleInstanceResponse&
                internal_response) {
            mmo::public_api::SettleInstanceResponse response;
            *response.mutable_context() =
                protocol::make_ok_context(request.context());
            response.set_reward_grant_id(internal_response.reward_grant_id());
            response.set_duplicate(internal_response.duplicate());
            for (const auto& reward : internal_response.rewards()) {
                *response.add_rewards() = reward;
            }
            return response;
        };
    gateway::bind_proxy_handler<
        mmo::public_api::SettleInstanceRequest,
        mmo::internal_api::GatewaySettleInstanceRequest,
        mmo::internal_api::GatewaySettleInstanceResponse,
        mmo::public_api::SettleInstanceResponse>(
        gateway_router,
        app_proto::kSettleInstanceRequest,
        context,
        std::move(settle_mapper));
}

}  // namespace apps::game_gateway_server

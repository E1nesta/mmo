#include "apps/instance_server/instance_handlers.h"

#include <utility>

#include "internal/gateway_instance.pb.h"
#include "internal/instance_player.pb.h"
#include "runtime/channel/routing_policy.h"
#include "runtime/protocol/envelope_utils.h"
#include "apps/protocol/message_types.h"
#include "runtime/rpc/rpc_controller.h"
#include "runtime/rpc/rpc_error.h"
#include "runtime/rpc/rpc_result.h"
#include "runtime/server/handler_result.h"
#include "runtime/server/typed_handler.h"

namespace mmo::apps::instance_server {

void register_instance_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::instance::InstanceService& service,
    mmo::runtime::rpc::RpcClient& rpc_client,
    const std::string& service_name) {
    mmo::runtime::server::bind_typed_handler<
        mmo::internal_api::GatewayEnterInstanceRequest,
        mmo::internal_api::GatewayEnterInstanceResponse>(
        rpc_server,
        mmo::apps::protocol::kGatewayEnterInstanceRequest,
        mmo::apps::protocol::kGatewayEnterInstanceResponse,
        service_name,
        [&service](
            const mmo::internal_api::GatewayEnterInstanceRequest& request,
            const mmo::runtime::server::ServiceContext&) {
            const auto instance = service.enter_instance(
                request.context().player_id(),
                request.dungeon_id());

            mmo::internal_api::GatewayEnterInstanceResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_instance_id(instance.instance_id);
            response.set_boss_entity_id(instance.boss_entity_id);

            return mmo::runtime::server::HandlerResult<
                mmo::internal_api::GatewayEnterInstanceResponse>::success(
                    std::move(response));
        });

    mmo::runtime::server::bind_typed_handler<
        mmo::internal_api::GatewaySettleInstanceRequest,
        mmo::internal_api::GatewaySettleInstanceResponse>(
        rpc_server,
        mmo::apps::protocol::kGatewaySettleInstanceRequest,
        mmo::apps::protocol::kGatewaySettleInstanceResponse,
        service_name,
        [&service, &rpc_client](
            const mmo::internal_api::GatewaySettleInstanceRequest& request,
            const mmo::runtime::server::ServiceContext& context) {
            const auto settled = service.settle_instance(
                request.context().player_id(),
                request.instance_id(),
                request.idempotency_key(),
                request.win());

            mmo::internal_api::GrantInstanceRewardRequest reward_request;
            *reward_request.mutable_context() = request.context();
            reward_request.set_reward_grant_id(settled.reward_grant_id);
            for (const auto& reward : settled.rewards) {
                auto* proto_reward = reward_request.add_rewards();
                proto_reward->set_type(reward.type);
                proto_reward->set_amount(reward.amount);
            }

            mmo::runtime::rpc::RpcController controller;
            controller.source_service = context.service_name;
            controller.routing_policy =
                mmo::runtime::channel::RoutingPolicy::kStickyPlayer;
            const auto rpc_result = rpc_client.call(
                "player_server",
                mmo::apps::protocol::kGrantInstanceRewardRequest,
                request.context(),
                reward_request,
                controller);
            if (!rpc_result.ok()) {
                return mmo::runtime::server::HandlerResult<
                    mmo::internal_api::GatewaySettleInstanceResponse>::failure(
                        mmo::runtime::rpc::rpc_error_to_status_code(
                            rpc_result.error().code),
                        rpc_result.error().message.empty()
                            ? "player rpc failed"
                            : rpc_result.error().message);
            }

            mmo::internal_api::GrantInstanceRewardResponse reward_response;
            if (!mmo::runtime::protocol::unpack_message(
                    rpc_result.response(), reward_response)) {
                return mmo::runtime::server::HandlerResult<
                    mmo::internal_api::GatewaySettleInstanceResponse>::failure(
                        502, "invalid player reward response");
            }
            if (!reward_response.context().success()) {
                return mmo::runtime::server::HandlerResult<
                    mmo::internal_api::GatewaySettleInstanceResponse>::failure(
                        reward_response.context().error_code(),
                        reward_response.context().error_message());
            }

            mmo::internal_api::GatewaySettleInstanceResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_reward_grant_id(settled.reward_grant_id);
            response.set_duplicate(settled.duplicate);
            for (const auto& reward : settled.rewards) {
                auto* proto_reward = response.add_rewards();
                proto_reward->set_type(reward.type);
                proto_reward->set_amount(reward.amount);
            }

            return mmo::runtime::server::HandlerResult<
                mmo::internal_api::GatewaySettleInstanceResponse>::success(
                    std::move(response));
        });
}

}  // namespace mmo::apps::instance_server

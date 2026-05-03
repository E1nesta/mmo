#include "apps/instance_server/instance_handlers.h"

#include "internal/gateway_instance.pb.h"
#include "internal/instance_player.pb.h"
#include "runtime/channel/routing_policy.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/rpc/rpc_controller.h"
#include "runtime/rpc/rpc_result.h"

namespace mmo::apps::instance_server {

void register_instance_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::instance::InstanceService& service,
    mmo::runtime::rpc::RpcClient& rpc_client,
    const std::string& service_name) {
    rpc_server.on(
        mmo::runtime::protocol::kGatewayEnterInstanceRequest,
        [&service](const mmo::common::Envelope& envelope) {
            mmo::internal_api::GatewayEnterInstanceRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid enter instance request");
            }

            const auto instance = service.enter_instance(
                request.context().player_id(),
                request.dungeon_id());

            mmo::internal_api::GatewayEnterInstanceResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_instance_id(instance.instance_id);
            response.set_boss_entity_id(instance.boss_entity_id);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGatewayEnterInstanceResponse,
                request.context(),
                response);
        });

    rpc_server.on(
        mmo::runtime::protocol::kGatewaySettleInstanceRequest,
        [&service, &rpc_client, service_name](
            const mmo::common::Envelope& envelope) {
            mmo::internal_api::GatewaySettleInstanceRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid settle instance request");
            }

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
            controller.source_service = service_name;
            controller.routing_policy =
                mmo::runtime::channel::RoutingPolicy::kStickyPlayer;
            const auto rpc_result = rpc_client.call(
                "player_server",
                mmo::runtime::protocol::kGrantInstanceRewardRequest,
                request.context(),
                reward_request,
                controller);
            if (!rpc_result.ok()) {
                return rpc_result.make_error_envelope(envelope);
            }

            mmo::internal_api::GrantInstanceRewardResponse reward_response;
            if (!mmo::runtime::protocol::unpack_message(
                    rpc_result.response(), reward_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, "invalid player reward response");
            }
            if (!reward_response.context().success()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
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

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGatewaySettleInstanceResponse,
                request.context(),
                response);
        });
}

}  // namespace mmo::apps::instance_server

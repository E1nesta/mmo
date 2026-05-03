#include "apps/player_server/player_handlers.h"

#include <vector>

#include <google/protobuf/repeated_ptr_field.h>

#include "common/types.pb.h"
#include "internal/gateway_player.pb.h"
#include "internal/instance_player.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"

namespace mmo::apps::player_server {
namespace {

std::vector<mmo::modules::player::Reward> to_rewards(
    const google::protobuf::RepeatedPtrField<mmo::common::Reward>& rewards) {
    std::vector<mmo::modules::player::Reward> result;
    for (const auto& reward : rewards) {
        result.push_back(
            mmo::modules::player::Reward{reward.type(), reward.amount()});
    }
    return result;
}

}  // namespace

void register_player_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::player::PlayerService& service) {
    rpc_server.on(
        mmo::runtime::protocol::kGrantInstanceRewardRequest,
        [&service](const mmo::common::Envelope& envelope) {
            mmo::internal_api::GrantInstanceRewardRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid grant instance reward request");
            }

            const auto applied = service.apply_reward(
                request.context().player_id(),
                request.reward_grant_id(),
                to_rewards(request.rewards()));
            if (!applied.success) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    applied.error_code == 0 ? 500 : applied.error_code,
                    applied.error_message.empty()
                        ? "failed to apply reward"
                        : applied.error_message);
            }

            mmo::internal_api::GrantInstanceRewardResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_applied(applied.applied);
            response.set_gold(applied.gold);
            response.set_exp(applied.exp);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGrantInstanceRewardResponse,
                request.context(),
                response);
        });

    rpc_server.on(
        mmo::runtime::protocol::kGatewayApplyRewardRequest,
        [&service](const mmo::common::Envelope& envelope) {
            mmo::internal_api::GatewayApplyRewardRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid apply reward request");
            }

            const auto applied = service.apply_reward(
                request.context().player_id(),
                request.idempotency_key(),
                to_rewards(request.rewards()));
            if (!applied.success) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    applied.error_code == 0 ? 500 : applied.error_code,
                    applied.error_message.empty()
                        ? "failed to apply reward"
                        : applied.error_message);
            }

            mmo::internal_api::GatewayApplyRewardResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_applied(applied.applied);
            response.set_gold(applied.gold);
            response.set_exp(applied.exp);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGatewayApplyRewardResponse,
                request.context(),
                response);
        });
}

}  // namespace mmo::apps::player_server

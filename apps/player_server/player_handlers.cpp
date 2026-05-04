#include "apps/player_server/player_handlers.h"

#include <vector>
#include <utility>

#include <google/protobuf/repeated_ptr_field.h>

#include "common/types.pb.h"
#include "internal/gateway_player.pb.h"
#include "internal/instance_player.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "apps/protocol/message_types.h"
#include "runtime/server/handler_result.h"
#include "runtime/server/typed_handler.h"

namespace apps::player_server {

namespace app_proto = apps::protocol;
namespace protocol = runtime::protocol;
namespace rpc = runtime::rpc;
namespace server = runtime::server;
namespace player = modules::player;
namespace {

std::vector<player::Reward> to_rewards(
    const google::protobuf::RepeatedPtrField<mmo::common::Reward>& rewards) {
    std::vector<player::Reward> result;
    for (const auto& reward : rewards) {
        result.push_back(
            player::Reward{reward.type(), reward.amount()});
    }
    return result;
}

}  // namespace

void register_player_handlers(
    rpc::RpcServer& rpc_server,
    player::PlayerService& service) {
    server::bind_typed_handler<
        mmo::internal_api::GrantInstanceRewardRequest,
        mmo::internal_api::GrantInstanceRewardResponse>(
        rpc_server,
        app_proto::kGrantInstanceRewardRequest,
        app_proto::kGrantInstanceRewardResponse,
        "player_server",
        [&service](
            const mmo::internal_api::GrantInstanceRewardRequest& request,
            const server::ServiceContext&) {
            const auto applied = service.apply_reward(
                request.context().player_id(),
                request.reward_grant_id(),
                to_rewards(request.rewards()));
            if (!applied.success) {
                return server::HandlerResult<
                    mmo::internal_api::GrantInstanceRewardResponse>::failure(
                    applied.error_code == 0 ? 500 : applied.error_code,
                    applied.error_message.empty()
                        ? "failed to apply reward"
                        : applied.error_message);
            }

            mmo::internal_api::GrantInstanceRewardResponse response;
            *response.mutable_context() =
                protocol::make_ok_context(request.context());
            response.set_applied(applied.applied);
            response.set_gold(applied.gold);
            response.set_exp(applied.exp);

            return server::HandlerResult<
                mmo::internal_api::GrantInstanceRewardResponse>::success(
                    std::move(response));
        });

    server::bind_typed_handler<
        mmo::internal_api::GatewayApplyRewardRequest,
        mmo::internal_api::GatewayApplyRewardResponse>(
        rpc_server,
        app_proto::kGatewayApplyRewardRequest,
        app_proto::kGatewayApplyRewardResponse,
        "player_server",
        [&service](
            const mmo::internal_api::GatewayApplyRewardRequest& request,
            const server::ServiceContext&) {
            const auto applied = service.apply_reward(
                request.context().player_id(),
                request.idempotency_key(),
                to_rewards(request.rewards()));
            if (!applied.success) {
                return server::HandlerResult<
                    mmo::internal_api::GatewayApplyRewardResponse>::failure(
                    applied.error_code == 0 ? 500 : applied.error_code,
                    applied.error_message.empty()
                        ? "failed to apply reward"
                        : applied.error_message);
            }

            mmo::internal_api::GatewayApplyRewardResponse response;
            *response.mutable_context() =
                protocol::make_ok_context(request.context());
            response.set_applied(applied.applied);
            response.set_gold(applied.gold);
            response.set_exp(applied.exp);

            return server::HandlerResult<
                mmo::internal_api::GatewayApplyRewardResponse>::success(
                    std::move(response));
        });
}

}  // namespace apps::player_server

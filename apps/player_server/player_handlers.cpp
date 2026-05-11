#include "apps/player_server/player_handlers.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <utility>

#include <google/protobuf/repeated_ptr_field.h>

#include "common/types.pb.h"
#include "cs/player.pb.h"
#include "ss/instance_player.pb.h"
#include "runtime/entity/entity_id.h"
#include "runtime/entity/entity_mailbox.h"
#include "runtime/entity/entity_state_store.h"
#include "proto/message_catalog.h"
#include "runtime/handler/handler_result.h"
#include "runtime/handler/typed_handler.h"
#include "runtime/protocol/payload_utils.h"

namespace apps::player_server {

namespace app_proto = mmo::protocol;
namespace protocol = runtime::protocol;
namespace rpc = runtime::rpc;
namespace handler = runtime::handler;
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
    rpc::RpcDispatcher& dispatcher,
    player::PlayerService& service,
    runtime::entity::EntityExecutor& entity_executor,
    runtime::scheduler::ShardedExecutor& entity_scheduler) {
    using PlayerStateStore =
        runtime::entity::EntityStateStore<std::int64_t, player::PlayerState>;
    auto player_states = std::make_shared<PlayerStateStore>();

    handler::bind_async_typed_handler<
        mmo::ss::GrantInstanceRewardRequest,
        mmo::ss::GrantInstanceRewardResponse>(
        dispatcher,
        app_proto::kGrantInstanceRewardRequest,
        app_proto::kGrantInstanceRewardResponse,
        "player_server",
        [&service,
         &entity_executor,
         &entity_scheduler,
         player_states](
            const mmo::ss::GrantInstanceRewardRequest& request,
            const handler::HandlerContext& context,
            auto reply) {
            const auto player_id = static_cast<std::int64_t>(context.route_key);
            if (player_id <= 0) {
                reply(handler::HandlerResult<
                    mmo::ss::GrantInstanceRewardResponse>::failure(
                        401, "player route key is required"));
                return;
            }

            const auto entity_id =
                runtime::entity::player_entity(player_id);
            auto reply_once = std::make_shared<decltype(reply)>(std::move(reply));
            auto player_state = player_states->find_or_create(player_id);
            auto drain = entity_executor.submit(
                entity_scheduler,
                runtime::entity::EntityMessage{
                    entity_id,
                    context.message_id,
                    context.request_id,
                    static_cast<std::uint64_t>(player_id)},
                [&service,
                 request,
                 player_id,
                 player_state,
                 reply_once](
                    const runtime::entity::EntityMessage&) mutable {
                    const auto applied = service.apply_reward(
                        *player_state,
                        player_id,
                        request.reward_grant_id(),
                        to_rewards(request.rewards()));
                    if (!applied.success) {
                        (*reply_once)(handler::HandlerResult<
                            mmo::ss::GrantInstanceRewardResponse>::failure(
                            applied.error_code == 0 ? 500 : applied.error_code,
                            applied.error_message.empty()
                                ? "failed to apply reward"
                                : applied.error_message));
                        return;
                    }

                    mmo::ss::GrantInstanceRewardResponse response;
                    *response.mutable_result() = protocol::make_ok_result();
                    response.set_applied(applied.applied);
                    response.set_gold(applied.gold);
                    response.set_exp(applied.exp);
                    (*reply_once)(handler::HandlerResult<
                        mmo::ss::GrantInstanceRewardResponse>::success(
                            std::move(response)));
                });
            if (!drain.accepted()) {
                (*reply_once)(handler::HandlerResult<
                    mmo::ss::GrantInstanceRewardResponse>::failure(
                        503, "player entity executor is unavailable"));
                return;
            }
        });

    handler::bind_async_typed_handler<
        mmo::cs::ApplyRewardRequest,
        mmo::cs::ApplyRewardResponse>(
        dispatcher,
        app_proto::kApplyRewardRequest,
        app_proto::kApplyRewardResponse,
        "player_server",
        [&service,
         &entity_executor,
         &entity_scheduler,
         player_states](
            const mmo::cs::ApplyRewardRequest& request,
            const handler::HandlerContext& context,
            auto reply) {
            const auto player_id = static_cast<std::int64_t>(context.route_key);
            if (player_id <= 0) {
                reply(handler::HandlerResult<
                    mmo::cs::ApplyRewardResponse>::failure(
                        401, "player route key is required"));
                return;
            }

            const auto entity_id =
                runtime::entity::player_entity(player_id);
            auto reply_once = std::make_shared<decltype(reply)>(std::move(reply));
            auto player_state = player_states->find_or_create(player_id);
            auto drain = entity_executor.submit(
                entity_scheduler,
                runtime::entity::EntityMessage{
                    entity_id,
                    context.message_id,
                    context.request_id,
                    static_cast<std::uint64_t>(player_id)},
                [&service,
                 request,
                 player_id,
                 player_state,
                 reply_once](
                    const runtime::entity::EntityMessage&) mutable {
                    const auto applied = service.apply_reward(
                        *player_state,
                        player_id,
                        request.idempotency_key(),
                        to_rewards(request.rewards()));
                    if (!applied.success) {
                        (*reply_once)(handler::HandlerResult<
                            mmo::cs::ApplyRewardResponse>::failure(
                            applied.error_code == 0 ? 500 : applied.error_code,
                            applied.error_message.empty()
                                ? "failed to apply reward"
                                : applied.error_message));
                        return;
                    }

                    mmo::cs::ApplyRewardResponse response;
                    *response.mutable_result() = protocol::make_ok_result();
                    response.set_applied(applied.applied);
                    response.set_gold(applied.gold);
                    response.set_exp(applied.exp);
                    (*reply_once)(handler::HandlerResult<
                        mmo::cs::ApplyRewardResponse>::success(
                            std::move(response)));
                });
            if (!drain.accepted()) {
                (*reply_once)(handler::HandlerResult<
                    mmo::cs::ApplyRewardResponse>::failure(
                        503, "player entity executor is unavailable"));
                return;
            }
        });
}

}  // namespace apps::player_server

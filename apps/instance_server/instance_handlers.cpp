#include "apps/instance_server/instance_handlers.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

#include "cs/instance.pb.h"
#include "ss/instance_player.pb.h"
#include "runtime/entity/entity_id.h"
#include "runtime/entity/entity_mailbox.h"
#include "runtime/entity/entity_state_store.h"
#include "runtime/rpc/rpc_routing_policy.h"
#include "proto/message_catalog.h"
#include "runtime/rpc/rpc_error.h"
#include "runtime/rpc/rpc_options.h"
#include "runtime/rpc/rpc_result.h"
#include "runtime/handler/handler_result.h"
#include "runtime/handler/typed_handler.h"
#include "runtime/protocol/payload_utils.h"

namespace apps::instance_server {

namespace app_proto = mmo::protocol;
namespace rpc = runtime::rpc;
namespace protocol = runtime::protocol;
namespace handler = runtime::handler;
namespace instance = modules::instance;

void register_instance_handlers(
    rpc::RpcDispatcher& dispatcher,
    instance::InstanceService& service,
    runtime::entity::EntityExecutor& entity_executor,
    runtime::scheduler::ShardedExecutor& entity_scheduler,
    rpc::RpcClient& rpc_client,
    const std::string& service_name) {
    auto next_instance_id = std::make_shared<std::atomic<std::int64_t>>(500000);
    using InstanceStateStore =
        runtime::entity::EntityStateStore<std::int64_t, instance::InstanceState>;
    auto instance_states = std::make_shared<InstanceStateStore>();

    handler::bind_async_typed_handler<
        mmo::cs::EnterInstanceRequest,
        mmo::cs::EnterInstanceResponse>(
        dispatcher,
        app_proto::kEnterInstanceRequest,
        app_proto::kEnterInstanceResponse,
        service_name,
        [&service,
         &entity_executor,
         &entity_scheduler,
         next_instance_id,
         instance_states](
            const mmo::cs::EnterInstanceRequest& request,
            const handler::HandlerContext& context,
            auto reply) {
            const auto player_id = static_cast<std::int64_t>(context.route_key);
            if (player_id <= 0) {
                reply(handler::HandlerResult<
                    mmo::cs::EnterInstanceResponse>::failure(
                        401, "player route key is required"));
                return;
            }
            if (request.dungeon_id() <= 0) {
                reply(handler::HandlerResult<
                    mmo::cs::EnterInstanceResponse>::failure(
                        400, "dungeon id is required"));
                return;
            }

            const auto instance_id = next_instance_id->fetch_add(1);
            const auto entity_id =
                runtime::entity::instance_entity(instance_id);
            auto reply_once = std::make_shared<decltype(reply)>(std::move(reply));
            auto instance_state = instance_states->find_or_create(
                instance_id,
                [instance_id](instance::InstanceState& state) {
                    state.context.instance_id = instance_id;
                });
            auto drain = entity_executor.submit(
                entity_scheduler,
                runtime::entity::EntityMessage{
                    entity_id,
                    context.message_id,
                    context.request_id,
                    static_cast<std::uint64_t>(instance_id)},
                [&service,
                 request,
                 player_id,
                 instance_id,
                 instance_state,
                 reply_once](
                    const runtime::entity::EntityMessage&) mutable {
                    const auto instance = service.enter_instance(
                        instance_id,
                        player_id,
                        request.dungeon_id());
                    instance_state->context = instance;

                    mmo::cs::EnterInstanceResponse response;
                    *response.mutable_result() = protocol::make_ok_result();
                    response.set_instance_id(instance.instance_id);
                    response.set_boss_entity_id(instance.boss_entity_id);
                    (*reply_once)(handler::HandlerResult<
                        mmo::cs::EnterInstanceResponse>::success(
                            std::move(response)));
                });
            if (!drain.accepted()) {
                (*reply_once)(handler::HandlerResult<
                    mmo::cs::EnterInstanceResponse>::failure(
                        503, "instance entity executor is unavailable"));
                return;
            }
        });

    handler::bind_async_typed_handler<
        mmo::cs::SettleInstanceRequest,
        mmo::cs::SettleInstanceResponse>(
        dispatcher,
        app_proto::kSettleInstanceRequest,
        app_proto::kSettleInstanceResponse,
        service_name,
        [&service,
         &rpc_client,
         &entity_executor,
         &entity_scheduler,
         instance_states](
            const mmo::cs::SettleInstanceRequest& request,
            const handler::HandlerContext& context,
            auto reply) {
            const auto player_id = static_cast<std::int64_t>(context.route_key);
            if (player_id <= 0) {
                reply(handler::HandlerResult<
                    mmo::cs::SettleInstanceResponse>::failure(
                        401, "player route key is required"));
                return;
            }
            if (request.instance_id() <= 0) {
                reply(handler::HandlerResult<
                    mmo::cs::SettleInstanceResponse>::failure(
                        400, "instance id is required"));
                return;
            }

            const auto entity_id =
                runtime::entity::instance_entity(request.instance_id());
            const auto source_service = context.service_name;
            auto reply_once = std::make_shared<decltype(reply)>(std::move(reply));
            auto instance_state = instance_states->find_or_create(
                request.instance_id(),
                [&request](instance::InstanceState& state) {
                    state.context.instance_id = request.instance_id();
                });
            auto drain = entity_executor.submit(
                entity_scheduler,
                runtime::entity::EntityMessage{
                    entity_id,
                    context.message_id,
                    context.request_id,
                    request.instance_id()},
                [&service,
                 &rpc_client,
                 request,
                 source_service,
                 player_id,
                 instance_state,
                 reply_once](
                    const runtime::entity::EntityMessage&) mutable {
                    const auto settled = service.settle_instance(
                        *instance_state,
                        player_id,
                        request.idempotency_key(),
                        request.win());

                    mmo::ss::GrantInstanceRewardRequest reward_request;
                    reward_request.set_reward_grant_id(settled.reward_grant_id);
                    for (const auto& reward : settled.rewards) {
                        auto* proto_reward = reward_request.add_rewards();
                        proto_reward->set_type(reward.type);
                        proto_reward->set_amount(reward.amount);
                    }

                    rpc::RpcOptions controller;
                    controller.source_service = source_service;
                    controller.routing_policy =
                        rpc::RpcRoutingPolicy::kStickyRouteKey;
                    auto rpc_options = controller;
                    const auto rpc_error = rpc_client.call_async(
                        "player_server",
                        app_proto::kGrantInstanceRewardRequest,
                        static_cast<std::uint64_t>(player_id),
                        reward_request,
                        std::move(rpc_options),
                        [settled, reply_once](rpc::RpcResult rpc_result) mutable {
                            if (!rpc_result.ok()) {
                                (*reply_once)(handler::HandlerResult<
                                    mmo::cs::SettleInstanceResponse>::failure(
                                        rpc::rpc_error_to_status_code(
                                            rpc_result.error().code),
                                        rpc_result.error().message.empty()
                                            ? "player rpc failed"
                                            : rpc_result.error().message));
                                return;
                            }

                            mmo::ss::GrantInstanceRewardResponse reward_response;
                            if (!protocol::parse_payload(
                                    rpc_result.response(), &reward_response)) {
                                (*reply_once)(handler::HandlerResult<
                                    mmo::cs::SettleInstanceResponse>::failure(
                                        502, "invalid player reward response"));
                                return;
                            }
                            if (!reward_response.result().ok()) {
                                (*reply_once)(handler::HandlerResult<
                                    mmo::cs::SettleInstanceResponse>::failure(
                                        reward_response.result().error().code(),
                                        reward_response.result().error().message()));
                                return;
                            }

                            mmo::cs::SettleInstanceResponse response;
                            *response.mutable_result() = protocol::make_ok_result();
                            response.set_reward_grant_id(settled.reward_grant_id);
                            response.set_duplicate(settled.duplicate);
                            for (const auto& reward : settled.rewards) {
                                auto* proto_reward = response.add_rewards();
                                proto_reward->set_type(reward.type);
                                proto_reward->set_amount(reward.amount);
                            }

                            (*reply_once)(handler::HandlerResult<
                                mmo::cs::SettleInstanceResponse>::success(
                                    std::move(response)));
                        });
                    if (!rpc_error.ok()) {
                        (*reply_once)(handler::HandlerResult<
                            mmo::cs::SettleInstanceResponse>::failure(
                                rpc::rpc_error_to_status_code(rpc_error.code),
                                rpc_error.message.empty()
                                    ? "player rpc failed"
                                    : rpc_error.message));
                        return;
                    }
                });
            if (!drain.accepted()) {
                (*reply_once)(handler::HandlerResult<
                    mmo::cs::SettleInstanceResponse>::failure(
                        503, "instance entity executor is unavailable"));
                return;
            }
        });
}

}  // namespace apps::instance_server

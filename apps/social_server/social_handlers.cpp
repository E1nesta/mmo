#include "apps/social_server/social_handlers.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "cs/social.pb.h"
#include "runtime/entity/entity_id.h"
#include "runtime/entity/entity_mailbox.h"
#include "proto/message_catalog.h"
#include "runtime/handler/handler_result.h"
#include "runtime/handler/typed_handler.h"
#include "runtime/protocol/payload_utils.h"

namespace apps::social_server {

namespace app_proto = mmo::protocol;
namespace protocol = runtime::protocol;
namespace rpc = runtime::rpc;
namespace handler = runtime::handler;
namespace social = modules::social;

void register_social_handlers(
    rpc::RpcDispatcher& dispatcher,
    social::SocialBoundaryService& service,
    runtime::entity::EntityExecutor& entity_executor,
    runtime::scheduler::ShardedExecutor& entity_scheduler) {
    handler::bind_async_typed_handler<
        mmo::cs::SocialBoundaryRequest,
        mmo::cs::SocialBoundaryResponse>(
        dispatcher,
        app_proto::kSocialBoundaryRequest,
        app_proto::kSocialBoundaryResponse,
        "social_server",
        [&service, &entity_executor, &entity_scheduler](
            const mmo::cs::SocialBoundaryRequest& request,
            const handler::HandlerContext& context,
            auto reply) {
            const auto player_id = static_cast<std::int64_t>(context.route_key);
            if (player_id <= 0) {
                reply(handler::HandlerResult<
                    mmo::cs::SocialBoundaryResponse>::failure(
                        401, "player route key is required"));
                return;
            }
            if (request.target_player_id() <= 0) {
                reply(handler::HandlerResult<
                    mmo::cs::SocialBoundaryResponse>::failure(
                        400, "target player id is required"));
                return;
            }

            const auto entity_id =
                runtime::entity::chat_room_entity(request.target_player_id());
            auto reply_once = std::make_shared<decltype(reply)>(std::move(reply));
            auto drain = entity_executor.submit(
                entity_scheduler,
                runtime::entity::EntityMessage{
                    entity_id,
                    context.message_id,
                    context.request_id,
                    static_cast<std::uint64_t>(request.target_player_id())},
                [&service,
                 player_id,
                 reply_once](
                    const runtime::entity::EntityMessage&) mutable {
                    const auto boundary = service.boundary_for(player_id);

                    mmo::cs::SocialBoundaryResponse response;
                    *response.mutable_result() = protocol::make_ok_result();
                    response.set_friend_boundary_available(
                        boundary.friend_boundary_available);
                    response.set_chat_boundary_available(
                        boundary.chat_boundary_available);
                    response.set_team_boundary_available(
                        boundary.team_boundary_available);

                    (*reply_once)(handler::HandlerResult<
                        mmo::cs::SocialBoundaryResponse>::success(
                            std::move(response)));
                });
            if (!drain.accepted()) {
                (*reply_once)(handler::HandlerResult<
                    mmo::cs::SocialBoundaryResponse>::failure(
                        503, "chat room entity executor is unavailable"));
                return;
            }
        });
}

}  // namespace apps::social_server

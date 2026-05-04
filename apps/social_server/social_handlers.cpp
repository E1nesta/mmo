#include "apps/social_server/social_handlers.h"

#include <utility>

#include "internal/gateway_social.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/server/handler_result.h"
#include "runtime/server/typed_handler.h"

namespace mmo::apps::social_server {

void register_social_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::social::SocialBoundaryService& service) {
    mmo::runtime::server::bind_typed_handler<
        mmo::internal_api::GatewaySocialBoundaryRequest,
        mmo::internal_api::GatewaySocialBoundaryResponse>(
        rpc_server,
        mmo::runtime::protocol::kGatewaySocialBoundaryRequest,
        mmo::runtime::protocol::kGatewaySocialBoundaryResponse,
        "social_server",
        [&service](
            const mmo::internal_api::GatewaySocialBoundaryRequest& request,
            const mmo::runtime::server::ServiceContext&) {
            const auto boundary = service.boundary_for(request.context().player_id());

            mmo::internal_api::GatewaySocialBoundaryResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_friend_boundary_available(
                boundary.friend_boundary_available);
            response.set_chat_boundary_available(boundary.chat_boundary_available);
            response.set_team_boundary_available(boundary.team_boundary_available);

            return mmo::runtime::server::HandlerResult<
                mmo::internal_api::GatewaySocialBoundaryResponse>::success(
                    std::move(response));
        });
}

}  // namespace mmo::apps::social_server

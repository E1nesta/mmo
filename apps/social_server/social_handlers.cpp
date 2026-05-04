#include "apps/social_server/social_handlers.h"

#include <utility>

#include "internal/gateway_social.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "apps/protocol/message_types.h"
#include "runtime/server/handler_result.h"
#include "runtime/server/typed_handler.h"

namespace apps::social_server {

namespace app_proto = apps::protocol;
namespace protocol = runtime::protocol;
namespace rpc = runtime::rpc;
namespace server = runtime::server;
namespace social = modules::social;

void register_social_handlers(
    rpc::RpcServer& rpc_server,
    social::SocialBoundaryService& service) {
    server::bind_typed_handler<
        mmo::internal_api::GatewaySocialBoundaryRequest,
        mmo::internal_api::GatewaySocialBoundaryResponse>(
        rpc_server,
        app_proto::kGatewaySocialBoundaryRequest,
        app_proto::kGatewaySocialBoundaryResponse,
        "social_server",
        [&service](
            const mmo::internal_api::GatewaySocialBoundaryRequest& request,
            const server::ServiceContext&) {
            const auto boundary = service.boundary_for(request.context().player_id());

            mmo::internal_api::GatewaySocialBoundaryResponse response;
            *response.mutable_context() =
                protocol::make_ok_context(request.context());
            response.set_friend_boundary_available(
                boundary.friend_boundary_available);
            response.set_chat_boundary_available(boundary.chat_boundary_available);
            response.set_team_boundary_available(boundary.team_boundary_available);

            return server::HandlerResult<
                mmo::internal_api::GatewaySocialBoundaryResponse>::success(
                    std::move(response));
        });
}

}  // namespace apps::social_server

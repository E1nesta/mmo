#include "apps/social_server/social_handlers.h"

#include "internal/gateway_social.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"

namespace mmo::apps::social_server {

void register_social_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::social::SocialBoundaryService& service) {
    rpc_server.on(
        mmo::runtime::protocol::kGatewaySocialBoundaryRequest,
        [&service](const mmo::common::Envelope& envelope) {
            mmo::internal_api::GatewaySocialBoundaryRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid social boundary request");
            }

            const auto boundary = service.boundary_for(request.context().player_id());

            mmo::internal_api::GatewaySocialBoundaryResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_friend_boundary_available(
                boundary.friend_boundary_available);
            response.set_chat_boundary_available(boundary.chat_boundary_available);
            response.set_team_boundary_available(boundary.team_boundary_available);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGatewaySocialBoundaryResponse,
                request.context(),
                response);
        });
}

}  // namespace mmo::apps::social_server

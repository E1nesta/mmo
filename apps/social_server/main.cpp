#include <iostream>

#include "modules/social/social_boundary.h"
#include "public/social.pb.h"
#include "runtime/foundation/service_ports.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::modules::social::SocialBoundaryService service;

    mmo::runtime::transport::TcpEnvelopeServer server(
        mmo::runtime::foundation::kSocialServerPort,
        [&service](const mmo::public_api::Envelope& envelope) {
            if (envelope.message_type() !=
                mmo::runtime::protocol::kSocialBoundaryRequest) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "unsupported social message");
            }

            mmo::public_api::SocialBoundaryRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid social boundary request");
            }

            const auto boundary = service.boundary_for(request.context().player_id());

            mmo::public_api::SocialBoundaryResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_friend_boundary_available(
                boundary.friend_boundary_available);
            response.set_chat_boundary_available(boundary.chat_boundary_available);
            response.set_team_boundary_available(boundary.team_boundary_available);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kSocialBoundaryResponse,
                request.context(),
                response);
        });

    std::cout << "social_server starting\n";
    return server.run();
}

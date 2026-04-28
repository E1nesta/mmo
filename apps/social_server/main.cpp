#include <iostream>

#include "modules/social/social_boundary.h"
#include "public/social.pb.h"
#include "runtime/foundation/server_config.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    const std::string service_name = "social_server";
    const auto config = mmo::runtime::foundation::load_server_config_from_env();
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(config.transport.tcp);
    mmo::modules::social::SocialBoundaryService service;

    mmo::runtime::transport::TcpEnvelopeServer server(
        config.service(service_name).tcp_port,
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
        },
        service_name,
        tcp_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{service_name},
        "service_starting");
    return server.run();
}

#include "internal/gateway_social.pb.h"
#include "modules/social/social_boundary.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::runtime::foundation::ServerApp app("social_server");
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(
            app.config().transport.tcp, app.config().execution);

    mmo::modules::social::SocialBoundaryService service;
    mmo::runtime::rpc::RpcServer rpc_server(
        mmo::runtime::rpc::make_rpc_server_options(app.config()));

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

    mmo::runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        rpc_server.handler(),
        app.service_name(),
        tcp_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

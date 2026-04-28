#include <iostream>

#include "modules/gateway/gateway_session.h"
#include "public/gateway.pb.h"
#include "public/world.pb.h"
#include "runtime/foundation/server_config.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    const std::string service_name = "gateway_server";
    const auto config = mmo::runtime::foundation::load_server_config_from_env();
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(config.transport.tcp);
    mmo::modules::gateway::GatewaySessionRegistry sessions;

    mmo::runtime::transport::TcpEnvelopeServer server(
        config.service(service_name).tcp_port,
        [&sessions, &config, &tcp_options](const mmo::public_api::Envelope& envelope) {
            if (envelope.message_type() == mmo::runtime::protocol::kGateLoginRequest) {
                mmo::public_api::GateLoginRequest request;
                if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                    return mmo::runtime::protocol::make_error_envelope(
                        envelope, 400, "invalid gate login request");
                }

                const auto binding =
                    sessions.bind(request.player_id(), request.session_token());

                mmo::public_api::GateLoginResponse response;
                *response.mutable_context() =
                    mmo::runtime::protocol::make_ok_context(request.context());
                response.set_bound(true);
                response.set_connection_id(binding.connection_id);

                return mmo::runtime::protocol::pack_message(
                    mmo::runtime::protocol::kGateLoginResponse,
                    request.context(),
                    response);
            }

            if (envelope.message_type() == mmo::runtime::protocol::kEnterWorldRequest) {
                mmo::public_api::EnterWorldRequest request;
                if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                    return mmo::runtime::protocol::make_error_envelope(
                        envelope, 400, "invalid enter world request");
                }
                if (!sessions.is_bound(
                        request.context().player_id(),
                        request.context().session_token())) {
                    return mmo::runtime::protocol::make_error_envelope(
                        envelope, 401, "player is not bound to gateway");
                }

                return mmo::runtime::transport::send_envelope(
                    config.network.upstream_host,
                    config.service("world_server").tcp_port,
                    envelope,
                    tcp_options);
            }

            return mmo::runtime::protocol::make_error_envelope(
                envelope, 404, "unsupported gateway message");
        },
        service_name,
        tcp_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{service_name},
        "service_starting");
    return server.run();
}

#include <iostream>

#include "modules/gateway/gateway_session.h"
#include "public/gateway.pb.h"
#include "public/world.pb.h"
#include "runtime/foundation/service_ports.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::modules::gateway::GatewaySessionRegistry sessions;

    mmo::runtime::transport::TcpEnvelopeServer server(
        mmo::runtime::foundation::kGatewayServerPort,
        [&sessions](const mmo::public_api::Envelope& envelope) {
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
                    mmo::runtime::foundation::kLocalhost,
                    mmo::runtime::foundation::kWorldServerPort,
                    envelope);
            }

            return mmo::runtime::protocol::make_error_envelope(
                envelope, 404, "unsupported gateway message");
        });

    std::cout << "gateway_server starting\n";
    return server.run();
}

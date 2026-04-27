#include <iostream>

#include "modules/auth/auth_service.h"
#include "public/auth.pb.h"
#include "runtime/foundation/service_ports.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::modules::auth::AuthService service;

    mmo::runtime::transport::TcpEnvelopeServer server(
        mmo::runtime::foundation::kAuthServerPort,
        [&service](const mmo::public_api::Envelope& envelope) {
            if (envelope.message_type() != mmo::runtime::protocol::kLoginRequest) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "unsupported auth message");
            }

            mmo::public_api::LoginRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid login request");
            }

            const auto login =
                service.login(request.account_name(), request.device_id());

            mmo::public_api::LoginResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_account_id(login.account_id);
            response.set_player_id(login.player_id);
            response.set_session_token(login.session_token);
            response.set_expires_at_epoch_seconds(login.expires_at_epoch_seconds);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kLoginResponse, request.context(), response);
        });

    std::cout << "auth_server starting\n";
    return server.run();
}

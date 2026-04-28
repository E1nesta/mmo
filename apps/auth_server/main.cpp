#include "modules/auth/auth_service.h"
#include "public/auth.pb.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_router.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::runtime::foundation::ServerApp app("auth_server");
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(app.config().transport.tcp);

    mmo::modules::auth::AuthService service;
    mmo::runtime::protocol::MessageRouter router;

    router.on(
        mmo::runtime::protocol::kLoginRequest,
        [&service](const mmo::common::Envelope& envelope) {
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

    mmo::runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        router.handler(),
        app.service_name(),
        tcp_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

#include "runtime/rpc/rpc_server.h"

#include <utility>

#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/internal_auth.h"

namespace runtime::rpc {

RpcServerOptions make_rpc_server_options(
    const runtime::foundation::ServerConfig& config) {
    RpcServerOptions options;
    options.require_internal_auth = true;
    options.internal_auth_shared_secret =
        config.security.internal_auth.shared_secret;
    options.internal_auth_max_clock_skew_millis =
        config.security.internal_auth.max_clock_skew_millis;
    return options;
}

RpcServer::RpcServer(RpcServerOptions options)
    : options_(std::move(options)) {}

void RpcServer::on(std::string message_type, Handler handler) {
    router_.on(std::move(message_type), std::move(handler));
}

mmo::common::Envelope RpcServer::dispatch(
    const mmo::common::Envelope& envelope) const {
    if (options_.require_internal_auth) {
        std::string error_message;
        if (!runtime::protocol::validate_internal_envelope_now(
                envelope,
                options_.internal_auth_shared_secret,
                options_.internal_auth_max_clock_skew_millis,
                &error_message)) {
            metrics_.record_internal_auth_failed();
            return runtime::protocol::make_error_envelope(
                envelope, 401, "internal call authentication failed");
        }
    }
    return router_.dispatch(envelope);
}

RpcServer::Handler RpcServer::handler() const {
    return [this](const mmo::common::Envelope& envelope) {
        return dispatch(envelope);
    };
}

}  // namespace runtime::rpc

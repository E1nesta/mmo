#pragma once

#include <string>

#include "runtime/foundation/server_config.h"
#include "runtime/observability/metrics.h"
#include "runtime/protocol/message_router.h"
#include "runtime/transport/envelope_transport.h"

namespace mmo::runtime::rpc {

struct RpcServerOptions {
    bool require_internal_auth{true};
    std::string internal_auth_shared_secret;
    int internal_auth_max_clock_skew_millis{10000};
};

RpcServerOptions make_rpc_server_options(
    const mmo::runtime::foundation::ServerConfig& config);

class RpcServer {
public:
    using Handler = mmo::runtime::transport::EnvelopeHandler;

    explicit RpcServer(RpcServerOptions options = {});

    void on(std::string message_type, Handler handler);
    mmo::common::Envelope dispatch(const mmo::common::Envelope& envelope) const;
    Handler handler() const;

private:
    mmo::runtime::protocol::MessageRouter router_;
    RpcServerOptions options_;
    mutable mmo::runtime::observability::MetricsRegistry metrics_;
};

}  // namespace mmo::runtime::rpc

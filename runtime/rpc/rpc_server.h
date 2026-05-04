#pragma once

#include <string>

#include "runtime/foundation/server_config.h"
#include "runtime/observability/metrics.h"
#include "runtime/protocol/message_router.h"
#include "runtime/transport/envelope_transport.h"

namespace runtime::rpc {

struct RpcServerOptions {
    bool require_internal_auth{true};
    std::string internal_auth_shared_secret;
    int internal_auth_max_clock_skew_millis{10000};
};

RpcServerOptions make_rpc_server_options(
    const runtime::foundation::ServerConfig& config);

class RpcServer {
public:
    using Handler = runtime::transport::EnvelopeHandler;

    explicit RpcServer(RpcServerOptions options = {});

    void on(std::string message_type, Handler handler);
    mmo::common::Envelope dispatch(const mmo::common::Envelope& envelope) const;
    Handler handler() const;

private:
    runtime::protocol::MessageRouter router_;
    RpcServerOptions options_;
    mutable runtime::observability::MetricsRegistry metrics_;
};

}  // namespace runtime::rpc

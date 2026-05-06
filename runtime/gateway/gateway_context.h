#pragma once

#include <cstdint>

#include "runtime/gateway/gateway_route_table.h"
#include "runtime/observability/metrics.h"
#include "runtime/rpc/rpc_client.h"
#include "runtime/rpc/rpc_options.h"
#include "runtime/session/session_context.h"
#include "runtime/session/session_store.h"

namespace runtime::gateway {

struct GatewayContext {
    runtime::rpc::RpcClient& rpc_client;
    const GatewayRouteTable& route_table;
    runtime::session::SessionRegistry& sessions;
    runtime::session::SessionStore& session_store;
    runtime::observability::MetricsRegistry& security_metrics;
};

runtime::rpc::RpcOptions make_gateway_rpc_options(
    const GatewayRoute& route,
    std::uint64_t route_key = 0);

}  // namespace runtime::gateway

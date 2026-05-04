#pragma once

#include "runtime/observability/metrics.h"
#include "runtime/gateway/gateway_forwarder.h"
#include "runtime/gateway/gateway_router.h"
#include "runtime/gateway/proxy_route.h"
#include "runtime/session/session_context.h"
#include "runtime/session/session_store.h"

namespace apps::game_gateway_server {

void register_gateway_proxy_handlers(
    runtime::gateway::GatewayRouter& gateway_router,
    runtime::gateway::GatewayForwarder& forwarder,
    const runtime::gateway::ProxyRouteTable& route_table,
    runtime::session::SessionRegistry& sessions,
    runtime::session::SessionStore& session_store,
    runtime::observability::MetricsRegistry& security_metrics);

}  // namespace apps::game_gateway_server

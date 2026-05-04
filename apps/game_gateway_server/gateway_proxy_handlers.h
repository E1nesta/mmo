#pragma once

#include "runtime/observability/metrics.h"
#include "runtime/gateway/gateway_forwarder.h"
#include "runtime/gateway/gateway_router.h"
#include "runtime/gateway/proxy_route.h"
#include "runtime/session/session_context.h"
#include "runtime/session/session_store.h"

namespace mmo::apps::game_gateway_server {

void register_gateway_proxy_handlers(
    mmo::runtime::gateway::GatewayRouter& gateway_router,
    mmo::runtime::gateway::GatewayForwarder& forwarder,
    const mmo::runtime::gateway::ProxyRouteTable& route_table,
    mmo::runtime::session::SessionRegistry& sessions,
    mmo::runtime::session::SessionStore& session_store,
    mmo::runtime::observability::MetricsRegistry& security_metrics);

}  // namespace mmo::apps::game_gateway_server

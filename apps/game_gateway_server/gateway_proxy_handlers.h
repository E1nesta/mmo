#pragma once

#include "runtime/observability/metrics.h"
#include "runtime/routing/gateway_forwarder.h"
#include "runtime/routing/gateway_router.h"
#include "runtime/routing/route_table.h"
#include "runtime/session/redis_session_store.h"
#include "runtime/session/session_context.h"

namespace mmo::apps::game_gateway_server {

void register_gateway_proxy_handlers(
    mmo::runtime::routing::GatewayRouter& gateway_router,
    mmo::runtime::routing::GatewayForwarder& forwarder,
    const mmo::runtime::routing::RouteTable& route_table,
    mmo::runtime::session::SessionRegistry& sessions,
    mmo::runtime::session::RedisSessionStore& redis_sessions,
    mmo::runtime::observability::MetricsRegistry& security_metrics);

}  // namespace mmo::apps::game_gateway_server

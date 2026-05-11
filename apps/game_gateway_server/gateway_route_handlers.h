#pragma once

#include "runtime/rpc/rpc_client.h"
#include "runtime/observability/metrics.h"
#include "runtime/gateway/gateway_router.h"
#include "runtime/gateway/gateway_route_table.h"
#include "runtime/session/session_context.h"

namespace apps::game_gateway_server {

void register_gateway_route_handlers(
    runtime::gateway::GatewayRouter& gateway_router,
    runtime::rpc::RpcClient& rpc_client,
    const runtime::gateway::GatewayRouteTable& route_table,
    runtime::session::SessionRegistry& sessions,
    runtime::observability::MetricsRegistry& security_metrics);

}  // namespace apps::game_gateway_server

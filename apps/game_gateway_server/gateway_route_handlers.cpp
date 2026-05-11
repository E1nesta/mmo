#include "apps/game_gateway_server/gateway_route_handlers.h"

#include "runtime/gateway/gateway_route_handler.h"

namespace apps::game_gateway_server {

namespace gateway = runtime::gateway;
namespace rpc = runtime::rpc;
namespace observability = runtime::observability;
namespace session = runtime::session;

void register_gateway_route_handlers(
    gateway::GatewayRouter& gateway_router,
    rpc::RpcClient& rpc_client,
    const gateway::GatewayRouteTable& route_table,
    session::SessionRegistry& sessions,
    observability::MetricsRegistry& security_metrics) {
    gateway::GatewayContext context{
        rpc_client,
        route_table,
        sessions,
        security_metrics};
    for (const auto& route : route_table.routes()) {
        gateway::bind_gateway_route_handler(
            gateway_router, route.public_message_id, context);
    }
}

}  // namespace apps::game_gateway_server

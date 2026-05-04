#include "apps/game_gateway_server/gateway_proxy_handlers.h"

#include "apps/game_gateway_server/gateway_auth_proxy_handlers.h"
#include "apps/game_gateway_server/gateway_instance_proxy_handlers.h"
#include "apps/game_gateway_server/gateway_player_proxy_handlers.h"
#include "apps/game_gateway_server/gateway_social_proxy_handlers.h"
#include "apps/game_gateway_server/gateway_world_proxy_handlers.h"

namespace apps::game_gateway_server {

namespace gateway = runtime::gateway;
namespace observability = runtime::observability;
namespace session = runtime::session;

void register_gateway_proxy_handlers(
    gateway::GatewayRouter& gateway_router,
    gateway::GatewayForwarder& forwarder,
    const gateway::ProxyRouteTable& route_table,
    session::SessionRegistry& sessions,
    session::SessionStore& session_store,
    observability::MetricsRegistry& security_metrics) {
    gateway::ProxyContext context{
        forwarder,
        route_table,
        sessions,
        session_store,
        security_metrics};
    register_gateway_auth_proxy_handlers(gateway_router, context);
    register_gateway_world_proxy_handlers(gateway_router, context);
    register_gateway_instance_proxy_handlers(gateway_router, context);
    register_gateway_player_proxy_handlers(gateway_router, context);
    register_gateway_social_proxy_handlers(gateway_router, context);
}

}  // namespace apps::game_gateway_server

#include "apps/game_gateway_server/gateway_proxy_handlers.h"

#include "apps/game_gateway_server/gateway_auth_proxy_handlers.h"
#include "apps/game_gateway_server/gateway_instance_proxy_handlers.h"
#include "apps/game_gateway_server/gateway_player_proxy_handlers.h"
#include "apps/game_gateway_server/gateway_social_proxy_handlers.h"
#include "apps/game_gateway_server/gateway_world_proxy_handlers.h"

namespace mmo::apps::game_gateway_server {

void register_gateway_proxy_handlers(
    mmo::runtime::gateway::GatewayRouter& gateway_router,
    mmo::runtime::gateway::GatewayForwarder& forwarder,
    const mmo::runtime::gateway::ProxyRouteTable& route_table,
    mmo::runtime::session::SessionRegistry& sessions,
    mmo::runtime::session::SessionStore& session_store,
    mmo::runtime::observability::MetricsRegistry& security_metrics) {
    mmo::runtime::gateway::ProxyContext context{
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

}  // namespace mmo::apps::game_gateway_server

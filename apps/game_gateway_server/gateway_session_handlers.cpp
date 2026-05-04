#include "apps/game_gateway_server/gateway_session_handlers.h"

#include "apps/game_gateway_server/gateway_gate_login_handler.h"
#include "apps/game_gateway_server/gateway_ping_handler.h"
#include "apps/game_gateway_server/gateway_reconnect_handler.h"

namespace mmo::apps::game_gateway_server {

void register_gateway_session_handlers(
    mmo::runtime::gateway::GatewayRouter& gateway_router,
    const mmo::runtime::foundation::ServerConfig& config,
    mmo::runtime::session::SessionRegistry& sessions,
    mmo::runtime::session::TicketReplayStore& ticket_replay_guard,
    mmo::runtime::session::SessionStore& session_store,
    mmo::runtime::observability::MetricsRegistry& security_metrics) {
    mmo::runtime::gateway::GatewaySessionContext context{
        config,
        sessions,
        ticket_replay_guard,
        session_store,
        security_metrics};
    register_gateway_gate_login_handler(gateway_router, context);
    register_gateway_reconnect_handler(gateway_router, context);
    register_gateway_ping_handler(gateway_router, context);
}

}  // namespace mmo::apps::game_gateway_server

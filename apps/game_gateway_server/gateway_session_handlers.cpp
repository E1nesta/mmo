#include "apps/game_gateway_server/gateway_session_handlers.h"

#include "apps/game_gateway_server/gateway_gate_login_handler.h"
#include "apps/game_gateway_server/gateway_ping_handler.h"
#include "apps/game_gateway_server/gateway_reconnect_handler.h"

namespace apps::game_gateway_server {

namespace gateway = runtime::gateway;
namespace observability = runtime::observability;
namespace session = runtime::session;

void register_gateway_session_handlers(
    gateway::GatewayRouter& gateway_router,
    const runtime::foundation::ServerConfig& config,
    session::SessionRegistry& sessions,
    session::TicketReplayStore& ticket_replay_guard,
    session::SessionStore& session_store,
    observability::MetricsRegistry& security_metrics) {
    gateway::GatewaySessionContext context{
        config,
        sessions,
        ticket_replay_guard,
        session_store,
        security_metrics};
    register_gateway_gate_login_handler(gateway_router, context);
    register_gateway_reconnect_handler(gateway_router, context);
    register_gateway_ping_handler(gateway_router, context);
}

}  // namespace apps::game_gateway_server

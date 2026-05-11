#include "apps/game_gateway_server/gateway_session_handlers.h"

#include "apps/game_gateway_server/gateway_gate_login_handler.h"
#include "apps/game_gateway_server/gateway_ping_handler.h"
#include "apps/game_gateway_server/gateway_reconnect_handler.h"

namespace apps::game_gateway_server {

namespace gateway = runtime::gateway;
namespace observability = runtime::observability;
namespace session = runtime::session;

namespace {

gateway::GatewaySessionOptions make_gateway_session_options(
    const runtime::foundation::ServerConfig& config) {
    gateway::GatewaySessionOptions options;
    options.token_options.issuer = config.security.gateway_ticket.issuer;
    options.token_options.access_audience =
        config.security.gateway_ticket.access_audience;
    options.token_options.gateway_audience =
        config.security.gateway_ticket.gateway_audience;
    options.token_options.active_key_id =
        config.security.gateway_ticket.active_key_id;
    options.token_options.active_shared_secret =
        config.security.gateway_ticket.active_shared_secret;
    options.token_options.previous_key_id =
        config.security.gateway_ticket.previous_key_id;
    options.token_options.previous_shared_secret =
        config.security.gateway_ticket.previous_shared_secret;
    options.token_options.previous_key_accept_millis =
        config.security.gateway_ticket.previous_key_accept_millis;
    options.gateway_audience = config.security.gateway_ticket.gateway_audience;
    options.gateway_id = config.security.gateway_session.gateway_id;
    options.game_session_ttl_millis =
        config.security.gateway_session.game_session_ttl_millis;
    options.heartbeat_timeout_millis =
        config.security.gateway_session.heartbeat_timeout_millis;
    options.reconnect_ticket_ttl_millis =
        config.security.gateway_session.reconnect_ticket_ttl_millis;
    return options;
}

}  // namespace

void register_gateway_session_handlers(
    gateway::GatewayRouter& gateway_router,
    const runtime::foundation::ServerConfig& config,
    session::SessionRegistry& sessions,
    session::TicketReplayStore& ticket_replay_guard,
    session::SessionStore& session_store,
    observability::MetricsRegistry& security_metrics) {
    gateway::GatewaySessionContext context{
        make_gateway_session_options(config),
        sessions,
        ticket_replay_guard,
        session_store,
        security_metrics};
    register_gateway_gate_login_handler(gateway_router, context);
    register_gateway_reconnect_handler(gateway_router, context);
    register_gateway_ping_handler(gateway_router, context);
}

}  // namespace apps::game_gateway_server

#include <memory>

#include "apps/game_gateway_server/gateway_proxy_handlers.h"
#include "apps/game_gateway_server/gateway_route_table.h"
#include "apps/game_gateway_server/gateway_session_handlers.h"
#include "runtime/server/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/observability/metrics.h"
#include "runtime/gateway/gateway_forwarder.h"
#include "runtime/gateway/gateway_router.h"
#include "runtime/server/server_bootstrap.h"
#include "adapters/session_redis/redis_session_store.h"
#include "adapters/session_redis/redis_ticket_replay_store.h"
#include "runtime/session/session_context.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::runtime::server::ServerApp app("game_gateway_server");
    const auto tcp_options = mmo::runtime::server::make_server_transport_options(app);

    auto redis_pool = mmo::runtime::server::require_redis_pool(app);
    if (!redis_pool) {
        return 1;
    }

    mmo::runtime::gateway::GatewayForwarder forwarder(
        app.service_name(),
        app.config(),
        tcp_options);
    auto route_table = mmo::apps::game_gateway_server::make_gateway_route_table();

    mmo::runtime::session::SessionRegistry sessions;
    mmo::adapters::session_redis::RedisTicketReplayStore ticket_replay_guard(redis_pool);
    mmo::adapters::session_redis::RedisSessionStore redis_sessions(redis_pool);
    mmo::runtime::observability::MetricsRegistry security_metrics;
    mmo::runtime::gateway::GatewayRouter gateway_router;

    mmo::apps::game_gateway_server::register_gateway_proxy_handlers(
        gateway_router,
        forwarder,
        route_table,
        sessions,
        redis_sessions,
        security_metrics);
    mmo::apps::game_gateway_server::register_gateway_session_handlers(
        gateway_router,
        app.config(),
        sessions,
        ticket_replay_guard,
        redis_sessions,
        security_metrics);

    mmo::runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        gateway_router.handler(),
        app.service_name(),
        tcp_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

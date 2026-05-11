#include <memory>

#include "apps/game_gateway_server/gateway_route_table.h"
#include "apps/game_gateway_server/gateway_route_handlers.h"
#include "apps/game_gateway_server/gateway_session_handlers.h"
#include "runtime/service/service_app.h"
#include "runtime/observability/logging.h"
#include "runtime/observability/metrics.h"
#include "runtime/gateway/gateway_router.h"
#include "runtime/service/service_runtime.h"
#include "adapters/session_redis/redis_session_store.h"
#include "adapters/session_redis/redis_ticket_replay_store.h"
#include "runtime/session/session_context.h"
#include "runtime/net/reliable_frame_transport.h"

int main() {
    runtime::service::ServiceApp app("game_gateway_server");
    const auto tcp_options = runtime::service::make_server_transport_options(app);

    auto redis_pool = runtime::service::require_redis_pool(app);
    if (!redis_pool) {
        return 1;
    }

    auto rpc_client =
        runtime::service::make_rpc_client(app, tcp_options);
    auto route_table = apps::game_gateway_server::make_gateway_route_table();

    runtime::session::SessionRegistry sessions;
    adapters::session_redis::RedisTicketReplayStore ticket_replay_guard(redis_pool);
    adapters::session_redis::RedisSessionStore redis_sessions(redis_pool);
    runtime::observability::MetricsRegistry security_metrics;
    runtime::gateway::GatewayRouter gateway_router;

    apps::game_gateway_server::register_gateway_route_handlers(
        gateway_router,
        *rpc_client,
        route_table,
        sessions,
        security_metrics);
    apps::game_gateway_server::register_gateway_session_handlers(
        gateway_router,
        app.config(),
        sessions,
        ticket_replay_guard,
        redis_sessions,
        security_metrics);

    runtime::net::ReliableFrameTransport transport(
        app.service_config().tcp_port,
        gateway_router.handler(),
        app.service_name(),
        tcp_options);

    runtime::observability::log_info(
        runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return transport.run();
}

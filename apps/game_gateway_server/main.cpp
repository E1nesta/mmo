#include <memory>
#include <string>

#include "apps/game_gateway_server/gateway_proxy_handlers.h"
#include "apps/game_gateway_server/gateway_route_table.h"
#include "apps/game_gateway_server/gateway_session_handlers.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/observability/metrics.h"
#include "runtime/routing/gateway_forwarder.h"
#include "runtime/routing/gateway_router.h"
#include "runtime/session/redis_session_store.h"
#include "runtime/session/redis_ticket_replay_store.h"
#include "runtime/session/session_context.h"
#include "runtime/storage/storage_bootstrap.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::runtime::foundation::ServerApp app("game_gateway_server");
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(
            app.config().transport.tcp, app.config().execution);

    std::shared_ptr<mmo::runtime::storage::RedisConnectionPool> redis_pool;
    std::string storage_error;
    if (!mmo::runtime::storage::initialize_redis_pool(
            app.config(), &redis_pool, &storage_error)) {
        mmo::runtime::observability::log_error(
            mmo::runtime::observability::LogContext{app.service_name()},
            "redis_pool_init_failed error=" + storage_error);
        return 1;
    }

    mmo::runtime::routing::GatewayForwarder forwarder(
        app.service_name(),
        app.config(),
        tcp_options);
    auto route_table = mmo::apps::game_gateway_server::make_gateway_route_table();

    mmo::runtime::session::SessionRegistry sessions;
    mmo::runtime::session::RedisTicketReplayStore ticket_replay_guard(redis_pool);
    mmo::runtime::session::RedisSessionStore redis_sessions(redis_pool);
    mmo::runtime::observability::MetricsRegistry security_metrics;
    mmo::runtime::routing::GatewayRouter gateway_router;

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

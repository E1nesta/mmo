#include "runtime/bootstrap/service_bootstrap.h"

#include <memory>
#include <string>

#include "runtime/rpc/rpc_connection_pool.h"
#include "runtime/rpc/rpc_service_registry.h"
#include "runtime/observability/logging.h"
#include "runtime/storage/storage_bootstrap.h"
#include "runtime/net/tcp_frame_server.h"

namespace runtime::bootstrap {

runtime::net::TransportOptions make_server_transport_options(
    const ServiceApp& app) {
    return runtime::net::make_transport_options(
        app.config().transport.tcp, app.config().scheduler);
}

std::unique_ptr<runtime::rpc::RpcClient> make_rpc_client(
    const ServiceApp& app,
    const runtime::net::TransportOptions& transport_options) {
    auto service_registry =
        std::make_shared<runtime::rpc::StaticRpcServiceRegistry>(
            app.config());
    return std::make_unique<runtime::rpc::RpcClient>(
        service_registry,
        transport_options,
        runtime::rpc::make_rpc_connection_pool_options(
            app.config().rpc),
        runtime::rpc::make_rpc_client_options(
            app.service_name(), app.config()));
}

std::shared_ptr<runtime::storage::MysqlConnectionPool> require_mysql_pool(
    const ServiceApp& app) {
    std::shared_ptr<runtime::storage::MysqlConnectionPool> pool;
    std::string error_message;
    if (!runtime::storage::initialize_mysql_pool(
            app.config(), &pool, &error_message)) {
        runtime::observability::log_error(
            runtime::observability::LogContext{app.service_name()},
            "mysql_pool_init_failed error=" + error_message);
        return nullptr;
    }
    return pool;
}

std::shared_ptr<runtime::storage::RedisConnectionPool> require_redis_pool(
    const ServiceApp& app) {
    std::shared_ptr<runtime::storage::RedisConnectionPool> pool;
    std::string error_message;
    if (!runtime::storage::initialize_redis_pool(
            app.config(), &pool, &error_message)) {
        runtime::observability::log_error(
            runtime::observability::LogContext{app.service_name()},
            "redis_pool_init_failed error=" + error_message);
        return nullptr;
    }
    return pool;
}

int run_tcp_rpc_dispatcher(
    const ServiceApp& app,
    const runtime::rpc::RpcDispatcher& dispatcher,
    const runtime::net::TransportOptions& transport_options) {
    runtime::net::TcpFrameServer server(
        app.service_config().tcp_port,
        dispatcher.handler(),
        app.service_name(),
        transport_options);

    runtime::observability::log_info(
        runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

}  // namespace runtime::bootstrap

#include "runtime/server/server_bootstrap.h"

#include <memory>
#include <string>

#include "runtime/channel/channel_connection_pool.h"
#include "runtime/channel/service_registry.h"
#include "runtime/observability/logging.h"
#include "runtime/storage/storage_bootstrap.h"
#include "runtime/transport/tcp_envelope_server.h"

namespace runtime::server {

runtime::transport::TransportOptions make_server_transport_options(
    const ServerApp& app) {
    return runtime::transport::make_transport_options(
        app.config().transport.tcp, app.config().execution);
}

std::unique_ptr<runtime::rpc::RpcClient> make_static_rpc_client(
    const ServerApp& app,
    const runtime::transport::TransportOptions& transport_options) {
    auto service_registry =
        std::make_shared<runtime::channel::StaticServiceRegistry>(
            app.config());
    return std::make_unique<runtime::rpc::RpcClient>(
        service_registry,
        transport_options,
        runtime::channel::make_channel_connection_pool_options(
            app.config().channel),
        runtime::rpc::make_rpc_client_options(
            app.service_name(), app.config()));
}

std::shared_ptr<runtime::storage::MysqlConnectionPool> require_mysql_pool(
    const ServerApp& app) {
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
    const ServerApp& app) {
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

int run_tcp_rpc_server(
    const ServerApp& app,
    const runtime::rpc::RpcServer& rpc_server,
    const runtime::transport::TransportOptions& transport_options) {
    runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        rpc_server.handler(),
        app.service_name(),
        transport_options);

    runtime::observability::log_info(
        runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

}  // namespace runtime::server

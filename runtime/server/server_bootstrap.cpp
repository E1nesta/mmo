#include "runtime/server/server_bootstrap.h"

#include <memory>
#include <string>

#include "runtime/channel/channel_connection_pool.h"
#include "runtime/channel/service_registry.h"
#include "runtime/observability/logging.h"
#include "runtime/storage/storage_bootstrap.h"
#include "runtime/transport/tcp_envelope_server.h"

namespace mmo::runtime::server {

mmo::runtime::transport::TransportOptions make_server_transport_options(
    const ServerApp& app) {
    return mmo::runtime::transport::make_transport_options(
        app.config().transport.tcp, app.config().execution);
}

std::unique_ptr<mmo::runtime::rpc::RpcClient> make_static_rpc_client(
    const ServerApp& app,
    const mmo::runtime::transport::TransportOptions& transport_options) {
    auto service_registry =
        std::make_shared<mmo::runtime::channel::StaticServiceRegistry>(
            app.config());
    return std::make_unique<mmo::runtime::rpc::RpcClient>(
        service_registry,
        transport_options,
        mmo::runtime::channel::make_channel_connection_pool_options(
            app.config().channel),
        mmo::runtime::rpc::make_rpc_client_options(
            app.service_name(), app.config()));
}

std::shared_ptr<mmo::runtime::storage::MysqlConnectionPool> require_mysql_pool(
    const ServerApp& app) {
    std::shared_ptr<mmo::runtime::storage::MysqlConnectionPool> pool;
    std::string error_message;
    if (!mmo::runtime::storage::initialize_mysql_pool(
            app.config(), &pool, &error_message)) {
        mmo::runtime::observability::log_error(
            mmo::runtime::observability::LogContext{app.service_name()},
            "mysql_pool_init_failed error=" + error_message);
        return nullptr;
    }
    return pool;
}

std::shared_ptr<mmo::runtime::storage::RedisConnectionPool> require_redis_pool(
    const ServerApp& app) {
    std::shared_ptr<mmo::runtime::storage::RedisConnectionPool> pool;
    std::string error_message;
    if (!mmo::runtime::storage::initialize_redis_pool(
            app.config(), &pool, &error_message)) {
        mmo::runtime::observability::log_error(
            mmo::runtime::observability::LogContext{app.service_name()},
            "redis_pool_init_failed error=" + error_message);
        return nullptr;
    }
    return pool;
}

int run_tcp_rpc_server(
    const ServerApp& app,
    const mmo::runtime::rpc::RpcServer& rpc_server,
    const mmo::runtime::transport::TransportOptions& transport_options) {
    mmo::runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        rpc_server.handler(),
        app.service_name(),
        transport_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

}  // namespace mmo::runtime::server

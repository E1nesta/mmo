#pragma once

#include <memory>

#include "runtime/server/server_app.h"
#include "runtime/rpc/rpc_client.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/storage/mysql_connection_pool.h"
#include "runtime/storage/redis_connection_pool.h"
#include "runtime/transport/envelope_transport.h"

namespace runtime::server {

runtime::transport::TransportOptions make_server_transport_options(
    const ServerApp& app);

std::unique_ptr<runtime::rpc::RpcClient> make_static_rpc_client(
    const ServerApp& app,
    const runtime::transport::TransportOptions& transport_options);

std::shared_ptr<runtime::storage::MysqlConnectionPool> require_mysql_pool(
    const ServerApp& app);

std::shared_ptr<runtime::storage::RedisConnectionPool> require_redis_pool(
    const ServerApp& app);

int run_tcp_rpc_server(
    const ServerApp& app,
    const runtime::rpc::RpcServer& rpc_server,
    const runtime::transport::TransportOptions& transport_options);

}  // namespace runtime::server

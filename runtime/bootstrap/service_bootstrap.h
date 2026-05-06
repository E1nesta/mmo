#pragma once

#include <memory>

#include "runtime/bootstrap/service_app.h"
#include "runtime/rpc/rpc_client.h"
#include "runtime/rpc/rpc_dispatcher.h"
#include "runtime/storage/mysql_connection_pool.h"
#include "runtime/storage/redis_connection_pool.h"
#include "runtime/net/frame_transport.h"

namespace runtime::bootstrap {

runtime::net::TransportOptions make_server_transport_options(
    const ServiceApp& app);

std::unique_ptr<runtime::rpc::RpcClient> make_rpc_client(
    const ServiceApp& app,
    const runtime::net::TransportOptions& transport_options);

std::shared_ptr<runtime::storage::MysqlConnectionPool> require_mysql_pool(
    const ServiceApp& app);

std::shared_ptr<runtime::storage::RedisConnectionPool> require_redis_pool(
    const ServiceApp& app);

int run_tcp_rpc_dispatcher(
    const ServiceApp& app,
    const runtime::rpc::RpcDispatcher& dispatcher,
    const runtime::net::TransportOptions& transport_options);

}  // namespace runtime::bootstrap

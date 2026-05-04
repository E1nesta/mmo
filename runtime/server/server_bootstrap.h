#pragma once

#include <memory>

#include "runtime/server/server_app.h"
#include "runtime/rpc/rpc_client.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/transport/envelope_transport.h"

namespace mmo::runtime::server {

mmo::runtime::transport::TransportOptions make_server_transport_options(
    const ServerApp& app);

std::unique_ptr<mmo::runtime::rpc::RpcClient> make_static_rpc_client(
    const ServerApp& app,
    const mmo::runtime::transport::TransportOptions& transport_options);

int run_tcp_rpc_server(
    const ServerApp& app,
    const mmo::runtime::rpc::RpcServer& rpc_server,
    const mmo::runtime::transport::TransportOptions& transport_options);

}  // namespace mmo::runtime::server

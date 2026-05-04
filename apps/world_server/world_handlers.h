#pragma once

#include <string>

#include "modules/world/world_service.h"
#include "runtime/rpc/rpc_client.h"
#include "runtime/rpc/rpc_server.h"

namespace apps::world_server {

void register_world_handlers(
    runtime::rpc::RpcServer& rpc_server,
    modules::world::WorldService& service,
    runtime::rpc::RpcClient& rpc_client,
    const std::string& service_name);

}  // namespace apps::world_server

#pragma once

#include <string>

#include "modules/world/world_service.h"
#include "runtime/rpc/rpc_client.h"
#include "runtime/rpc/rpc_server.h"

namespace mmo::apps::world_server {

void register_world_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::world::WorldService& service,
    mmo::runtime::rpc::RpcClient& rpc_client,
    const std::string& service_name);

}  // namespace mmo::apps::world_server

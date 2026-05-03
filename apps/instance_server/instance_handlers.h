#pragma once

#include <string>

#include "modules/instance/instance_service.h"
#include "runtime/rpc/rpc_client.h"
#include "runtime/rpc/rpc_server.h"

namespace mmo::apps::instance_server {

void register_instance_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::instance::InstanceService& service,
    mmo::runtime::rpc::RpcClient& rpc_client,
    const std::string& service_name);

}  // namespace mmo::apps::instance_server

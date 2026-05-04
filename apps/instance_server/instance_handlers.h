#pragma once

#include <string>

#include "modules/instance/instance_service.h"
#include "runtime/rpc/rpc_client.h"
#include "runtime/rpc/rpc_server.h"

namespace apps::instance_server {

void register_instance_handlers(
    runtime::rpc::RpcServer& rpc_server,
    modules::instance::InstanceService& service,
    runtime::rpc::RpcClient& rpc_client,
    const std::string& service_name);

}  // namespace apps::instance_server

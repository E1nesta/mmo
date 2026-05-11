#pragma once

#include <string>

#include "modules/instance/instance_service.h"
#include "runtime/entity/entity_executor.h"
#include "runtime/rpc/rpc_client.h"
#include "runtime/rpc/rpc_dispatcher.h"
#include "runtime/scheduler/sharded_executor.h"

namespace apps::instance_server {

void register_instance_handlers(
    runtime::rpc::RpcDispatcher& dispatcher,
    modules::instance::InstanceService& service,
    runtime::entity::EntityExecutor& entity_executor,
    runtime::scheduler::ShardedExecutor& entity_scheduler,
    runtime::rpc::RpcClient& rpc_client,
    const std::string& service_name);

}  // namespace apps::instance_server

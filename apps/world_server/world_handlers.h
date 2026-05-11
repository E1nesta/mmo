#pragma once

#include <string>

#include "modules/world/world_service.h"
#include "runtime/entity/entity_executor.h"
#include "runtime/rpc/rpc_client.h"
#include "runtime/rpc/rpc_dispatcher.h"
#include "runtime/scheduler/sharded_executor.h"

namespace apps::world_server {

void register_world_handlers(
    runtime::rpc::RpcDispatcher& dispatcher,
    modules::world::WorldService& service,
    runtime::rpc::RpcClient& rpc_client,
    runtime::entity::EntityExecutor& entity_executor,
    runtime::scheduler::ShardedExecutor& entity_scheduler,
    const std::string& service_name);

}  // namespace apps::world_server

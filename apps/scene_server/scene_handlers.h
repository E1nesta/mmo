#pragma once

#include "modules/scene/scene_service.h"
#include "runtime/entity/entity_executor.h"
#include "runtime/rpc/rpc_dispatcher.h"
#include "runtime/scheduler/sharded_executor.h"

namespace apps::scene_server {

void register_scene_handlers(
    runtime::rpc::RpcDispatcher& dispatcher,
    modules::scene::SceneService& service,
    runtime::entity::EntityExecutor& entity_executor,
    runtime::scheduler::ShardedExecutor& entity_scheduler);

}  // namespace apps::scene_server

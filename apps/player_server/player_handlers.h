#pragma once

#include "modules/player/player_service.h"
#include "runtime/entity/entity_executor.h"
#include "runtime/rpc/rpc_dispatcher.h"
#include "runtime/scheduler/sharded_executor.h"

namespace apps::player_server {

void register_player_handlers(
    runtime::rpc::RpcDispatcher& dispatcher,
    modules::player::PlayerService& service,
    runtime::entity::EntityExecutor& entity_executor,
    runtime::scheduler::ShardedExecutor& entity_scheduler);

}  // namespace apps::player_server

#pragma once

#include "modules/social/social_boundary.h"
#include "runtime/entity/entity_executor.h"
#include "runtime/rpc/rpc_dispatcher.h"
#include "runtime/scheduler/sharded_executor.h"

namespace apps::social_server {

void register_social_handlers(
    runtime::rpc::RpcDispatcher& dispatcher,
    modules::social::SocialBoundaryService& service,
    runtime::entity::EntityExecutor& entity_executor,
    runtime::scheduler::ShardedExecutor& entity_scheduler);

}  // namespace apps::social_server

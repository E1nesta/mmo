#pragma once

#include "modules/social/social_boundary.h"
#include "runtime/rpc/rpc_server.h"

namespace apps::social_server {

void register_social_handlers(
    runtime::rpc::RpcServer& rpc_server,
    modules::social::SocialBoundaryService& service);

}  // namespace apps::social_server

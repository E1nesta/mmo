#pragma once

#include "modules/social/social_boundary.h"
#include "runtime/rpc/rpc_server.h"

namespace mmo::apps::social_server {

void register_social_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::social::SocialBoundaryService& service);

}  // namespace mmo::apps::social_server

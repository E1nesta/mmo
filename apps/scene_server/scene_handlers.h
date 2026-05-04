#pragma once

#include "modules/scene/scene_service.h"
#include "runtime/rpc/rpc_server.h"

namespace apps::scene_server {

void register_scene_handlers(
    runtime::rpc::RpcServer& rpc_server,
    modules::scene::SceneService& service);

}  // namespace apps::scene_server

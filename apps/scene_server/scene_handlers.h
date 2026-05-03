#pragma once

#include "modules/scene/scene_service.h"
#include "runtime/rpc/rpc_server.h"

namespace mmo::apps::scene_server {

void register_scene_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::scene::SceneService& service);

}  // namespace mmo::apps::scene_server

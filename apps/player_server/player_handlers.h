#pragma once

#include "modules/player/player_service.h"
#include "runtime/rpc/rpc_server.h"

namespace mmo::apps::player_server {

void register_player_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::player::PlayerService& service);

}  // namespace mmo::apps::player_server

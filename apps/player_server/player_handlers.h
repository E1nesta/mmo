#pragma once

#include "modules/player/player_service.h"
#include "runtime/rpc/rpc_server.h"

namespace apps::player_server {

void register_player_handlers(
    runtime::rpc::RpcServer& rpc_server,
    modules::player::PlayerService& service);

}  // namespace apps::player_server

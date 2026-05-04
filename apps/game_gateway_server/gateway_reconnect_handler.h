#pragma once

#include "runtime/gateway/gateway_session.h"
#include "runtime/gateway/gateway_router.h"

namespace apps::game_gateway_server {

void register_gateway_reconnect_handler(
    runtime::gateway::GatewayRouter& gateway_router,
    runtime::gateway::GatewaySessionContext& context);

}  // namespace apps::game_gateway_server

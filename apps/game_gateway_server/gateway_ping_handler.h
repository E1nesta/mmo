#pragma once

#include "runtime/gateway/gateway_session.h"
#include "runtime/gateway/gateway_router.h"

namespace mmo::apps::game_gateway_server {

void register_gateway_ping_handler(
    mmo::runtime::gateway::GatewayRouter& gateway_router,
    mmo::runtime::gateway::GatewaySessionContext& context);

}  // namespace mmo::apps::game_gateway_server

#pragma once

#include "runtime/gateway/proxy_context.h"

namespace mmo::apps::game_gateway_server {

void register_gateway_world_proxy_handlers(
    mmo::runtime::gateway::GatewayRouter& gateway_router,
    mmo::runtime::gateway::ProxyContext& context);

}  // namespace mmo::apps::game_gateway_server

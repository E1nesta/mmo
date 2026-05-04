#pragma once

#include "runtime/gateway/proxy_context.h"

namespace apps::game_gateway_server {

void register_gateway_world_proxy_handlers(
    runtime::gateway::GatewayRouter& gateway_router,
    runtime::gateway::ProxyContext& context);

}  // namespace apps::game_gateway_server

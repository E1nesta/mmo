#pragma once

#include "runtime/foundation/server_config.h"
#include "runtime/observability/metrics.h"
#include "runtime/gateway/gateway_router.h"
#include "runtime/session/session_context.h"
#include "runtime/session/session_store.h"
#include "runtime/session/ticket_replay_guard.h"

namespace apps::game_gateway_server {

void register_gateway_session_handlers(
    runtime::gateway::GatewayRouter& gateway_router,
    const runtime::foundation::ServerConfig& config,
    runtime::session::SessionRegistry& sessions,
    runtime::session::TicketReplayStore& ticket_replay_guard,
    runtime::session::SessionStore& session_store,
    runtime::observability::MetricsRegistry& security_metrics);

}  // namespace apps::game_gateway_server

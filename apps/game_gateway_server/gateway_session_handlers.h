#pragma once

#include "runtime/foundation/server_config.h"
#include "runtime/observability/metrics.h"
#include "runtime/gateway/gateway_router.h"
#include "runtime/session/session_context.h"
#include "runtime/session/session_store.h"
#include "runtime/session/ticket_replay_guard.h"

namespace mmo::apps::game_gateway_server {

void register_gateway_session_handlers(
    mmo::runtime::gateway::GatewayRouter& gateway_router,
    const mmo::runtime::foundation::ServerConfig& config,
    mmo::runtime::session::SessionRegistry& sessions,
    mmo::runtime::session::TicketReplayStore& ticket_replay_guard,
    mmo::runtime::session::SessionStore& session_store,
    mmo::runtime::observability::MetricsRegistry& security_metrics);

}  // namespace mmo::apps::game_gateway_server

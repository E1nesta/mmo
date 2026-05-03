#pragma once

#include <optional>

#include "common/context.pb.h"
#include "common/envelope.pb.h"
#include "runtime/foundation/server_config.h"
#include "runtime/observability/metrics.h"
#include "runtime/routing/gateway_router.h"
#include "runtime/session/redis_session_store.h"
#include "runtime/session/redis_ticket_replay_store.h"
#include "runtime/session/session_context.h"

namespace mmo::apps::game_gateway_server {

std::optional<mmo::common::Envelope> validate_bound_public_request(
    const mmo::common::Envelope& envelope,
    const mmo::runtime::session::SessionRegistry& sessions,
    const mmo::runtime::session::RedisSessionStore& redis_sessions,
    const mmo::common::RequestContext& context,
    mmo::runtime::observability::MetricsRegistry* metrics = nullptr);

void register_gateway_session_handlers(
    mmo::runtime::routing::GatewayRouter& gateway_router,
    const mmo::runtime::foundation::ServerConfig& config,
    mmo::runtime::session::SessionRegistry& sessions,
    mmo::runtime::session::RedisTicketReplayStore& ticket_replay_guard,
    mmo::runtime::session::RedisSessionStore& redis_sessions,
    mmo::runtime::observability::MetricsRegistry& security_metrics);

}  // namespace mmo::apps::game_gateway_server

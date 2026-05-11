#pragma once

#include <cstdint>
#include <string>

#include "runtime/observability/metrics.h"
#include "runtime/protocol/auth_tokens.h"
#include "runtime/session/session_context.h"
#include "runtime/session/session_store.h"
#include "runtime/session/ticket_replay_guard.h"

namespace runtime::gateway {

struct GatewaySessionOptions {
    runtime::protocol::AuthTokenOptions token_options;
    std::string gateway_audience;
    std::string gateway_id;
    int game_session_ttl_millis{};
    int heartbeat_timeout_millis{};
    int reconnect_ticket_ttl_millis{};
};

struct GatewaySessionContext {
    GatewaySessionOptions options;
    runtime::session::SessionRegistry& sessions;
    runtime::session::TicketReplayStore& ticket_replay_guard;
    runtime::session::SessionStore& session_store;
    runtime::observability::MetricsRegistry& security_metrics;
};

bool issue_reconnect_ticket(
    const GatewaySessionOptions& options,
    runtime::session::SessionStore& session_store,
    const runtime::session::ConnectionBinding& binding,
    std::uint64_t now_millis,
    std::string* reconnect_ticket,
    std::int64_t* reconnect_ticket_expires_at,
    std::string* error_message);

}  // namespace runtime::gateway

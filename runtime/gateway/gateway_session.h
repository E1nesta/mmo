#pragma once

#include <cstdint>
#include <string>

#include "runtime/foundation/server_config.h"
#include "runtime/observability/metrics.h"
#include "runtime/protocol/auth_tokens.h"
#include "runtime/session/session_context.h"
#include "runtime/session/session_store.h"
#include "runtime/session/ticket_replay_guard.h"

namespace runtime::gateway {

struct GatewaySessionContext {
    const runtime::foundation::ServerConfig& config;
    runtime::session::SessionRegistry& sessions;
    runtime::session::TicketReplayStore& ticket_replay_guard;
    runtime::session::SessionStore& session_store;
    runtime::observability::MetricsRegistry& security_metrics;
};

runtime::protocol::AuthTokenOptions make_gateway_token_options(
    const runtime::foundation::GatewayTicketConfig& config);

bool issue_reconnect_ticket(
    const runtime::foundation::ServerConfig& config,
    runtime::session::SessionStore& session_store,
    const runtime::session::ConnectionBinding& binding,
    std::uint64_t now_millis,
    std::string* reconnect_ticket,
    std::int64_t* reconnect_ticket_expires_at,
    std::string* error_message);

}  // namespace runtime::gateway

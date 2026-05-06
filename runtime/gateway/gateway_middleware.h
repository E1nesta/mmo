#pragma once

#include <optional>
#include <string>

#include "runtime/net/reliable_frame_codec.h"
#include "runtime/observability/metrics.h"
#include "runtime/session/session_context.h"
#include "runtime/session/session_store.h"

namespace runtime::gateway {

runtime::net::ReliableFrame make_gateway_error_frame(
    const runtime::net::ReliableFrame& request,
    int error_code,
    const std::string& error_message);

std::optional<runtime::net::ReliableFrame> validate_gateway_session(
    const runtime::net::ReliableFrame& frame,
    const runtime::session::SessionRegistry& sessions,
    const runtime::session::SessionStore& session_store,
    runtime::observability::MetricsRegistry* metrics = nullptr);

}  // namespace runtime::gateway

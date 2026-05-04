#pragma once

#include <optional>

#include "common/context.pb.h"
#include "common/envelope.pb.h"
#include "runtime/observability/metrics.h"
#include "runtime/session/session_context.h"
#include "runtime/session/session_store.h"

namespace runtime::gateway {

std::optional<mmo::common::Envelope> validate_gateway_session(
    const mmo::common::Envelope& envelope,
    const runtime::session::SessionRegistry& sessions,
    const runtime::session::SessionStore& session_store,
    const mmo::common::RequestContext& context,
    runtime::observability::MetricsRegistry* metrics = nullptr);

}  // namespace runtime::gateway

#include "runtime/gateway/gateway_middleware.h"

#include <cstdint>
#include <string>

#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/internal_auth.h"

namespace runtime::gateway {

std::optional<mmo::common::Envelope> validate_gateway_session(
    const mmo::common::Envelope& envelope,
    const runtime::session::SessionRegistry& sessions,
    const runtime::session::SessionStore& session_store,
    const mmo::common::RequestContext& context,
    runtime::observability::MetricsRegistry* metrics) {
    if (envelope.player_id() != context.player_id() ||
        envelope.session_token() != context.session_token() ||
        envelope.game_session_id() != context.game_session_id()) {
        return runtime::protocol::make_error_envelope(
            envelope, 400, "request context does not match envelope");
    }
    const auto now_millis = static_cast<std::uint64_t>(
        runtime::protocol::current_time_millis());
    std::string redis_error;
    if (envelope.game_session_id().empty() ||
        !sessions.is_bound(
            envelope.player_id(),
            envelope.session_token(),
            envelope.game_session_id(),
            now_millis) ||
        !session_store.is_bound(
            envelope.player_id(),
            envelope.session_token(),
            envelope.game_session_id(),
            now_millis,
            &redis_error)) {
        if (metrics != nullptr) {
            metrics->record_game_session_expired();
        }
        return runtime::protocol::make_error_envelope(
            envelope, 401, "game session is not bound to gateway");
    }
    return std::nullopt;
}

}  // namespace runtime::gateway

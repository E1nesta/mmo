#include "runtime/gateway/gateway_middleware.h"

#include <string>

#include "common/error.pb.h"
#include "runtime/protocol/frame.h"
#include "runtime/protocol/payload_utils.h"

namespace runtime::gateway {

runtime::net::ReliableFrame make_gateway_error_frame(
    const runtime::net::ReliableFrame& request,
    int error_code,
    const std::string& error_message) {
    runtime::net::ReliableFrame response;
    response.version = request.version;
    response.flags = request.flags;
    response.message_id = static_cast<std::uint16_t>(
        runtime::protocol::kErrorResponseMessageId);
    response.request_id = request.request_id;
    response.session_id = request.session_id;

    const auto result =
        runtime::protocol::make_error_result(error_code, error_message);
    std::string payload;
    result.SerializeToString(&payload);
    response.payload.assign(payload.begin(), payload.end());
    return response;
}

std::optional<runtime::net::ReliableFrame> validate_gateway_session(
    const runtime::net::ReliableFrame& frame,
    const runtime::session::SessionRegistry& sessions,
    runtime::observability::MetricsRegistry* metrics) {
    if (frame.session_id == 0) {
        if (metrics != nullptr) {
            metrics->record_game_session_expired();
        }
        return make_gateway_error_frame(
            frame, 401, "gateway session is required");
    }
    const auto binding = sessions.find_by_connection_id(frame.session_id);
    const auto now_millis =
        static_cast<std::uint64_t>(runtime::protocol::current_time_millis());
    if (!binding.has_value() || !binding->valid(now_millis)) {
        if (metrics != nullptr) {
            metrics->record_game_session_expired();
        }
        return make_gateway_error_frame(
            frame, 401, "gateway session is not bound");
    }
    return std::nullopt;
}

}  // namespace runtime::gateway

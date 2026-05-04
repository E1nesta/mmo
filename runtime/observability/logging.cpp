#include "runtime/observability/logging.h"

#include <sstream>

#include <spdlog/spdlog.h>

namespace runtime::observability {

std::string format_log_line(
    const LogContext& context,
    const std::string& event) {
    std::ostringstream output;
    output << "service=" << context.service_name
           << " event=" << event
           << " request_id=" << context.request_id
           << " account_id=" << context.account_id
           << " player_id=" << context.player_id;
    if (!context.message_type.empty()) {
        output << " message_type=" << context.message_type;
    }
    if (!context.trace_id.empty()) {
        output << " trace_id=" << context.trace_id;
    }
    if (!context.gateway_id.empty()) {
        output << " gateway_id=" << context.gateway_id;
    }
    if (!context.game_session_id.empty()) {
        output << " game_session_id=" << context.game_session_id;
    }
    if (!context.upstream.empty()) {
        output << " upstream=" << context.upstream;
    }
    if (!context.status.empty()) {
        output << " status=" << context.status;
    }
    if (context.error_code != 0) {
        output << " error_code=" << context.error_code;
    }
    if (context.latency_ms >= 0) {
        output << " latency_ms=" << context.latency_ms;
    }
    return output.str();
}

LogContext context_from_envelope(
    const std::string& service_name,
    const mmo::common::Envelope& envelope) {
    LogContext context;
    context.service_name = service_name;
    context.request_id = envelope.request_id();
    context.player_id = envelope.player_id();
    context.message_type = envelope.message_type();
    context.trace_id = envelope.trace_id();
    context.gateway_id =
        envelope.source_service().empty() ? service_name : envelope.source_service();
    context.game_session_id = envelope.game_session_id();
    return context;
}

void log_info(const LogContext& context, const std::string& event) {
    spdlog::info("{}", format_log_line(context, event));
}

void log_warn(const LogContext& context, const std::string& event) {
    spdlog::warn("{}", format_log_line(context, event));
}

void log_error(const LogContext& context, const std::string& event) {
    spdlog::error("{}", format_log_line(context, event));
}

}  // namespace runtime::observability

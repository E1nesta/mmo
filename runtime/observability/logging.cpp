#include "runtime/observability/logging.h"

#include <sstream>

#include <spdlog/spdlog.h>

namespace mmo::runtime::observability {
namespace {

std::string format_line(
    const LogContext& context,
    const std::string& event) {
    std::ostringstream output;
    output << "service_name=" << context.service_name
           << " event=" << event
           << " request_id=" << context.request_id
           << " player_id=" << context.player_id;
    if (!context.message_type.empty()) {
        output << " message_type=" << context.message_type;
    }
    if (context.error_code != 0) {
        output << " error_code=" << context.error_code;
    }
    if (context.latency_ms >= 0) {
        output << " latency_ms=" << context.latency_ms;
    }
    return output.str();
}

}  // namespace

LogContext context_from_envelope(
    const std::string& service_name,
    const mmo::public_api::Envelope& envelope) {
    LogContext context;
    context.service_name = service_name;
    context.request_id = envelope.request_id();
    context.player_id = envelope.player_id();
    context.message_type = envelope.message_type();
    return context;
}

void log_info(const LogContext& context, const std::string& event) {
    spdlog::info("{}", format_line(context, event));
}

void log_warn(const LogContext& context, const std::string& event) {
    spdlog::warn("{}", format_line(context, event));
}

void log_error(const LogContext& context, const std::string& event) {
    spdlog::error("{}", format_line(context, event));
}

}  // namespace mmo::runtime::observability

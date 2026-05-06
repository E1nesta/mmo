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
           << " route_key=" << context.route_key;
    if (context.message_id != 0U) {
        output << " message_id=" << context.message_id;
    }
    if (!context.gateway_id.empty()) {
        output << " gateway_id=" << context.gateway_id;
    }
    if (context.session_id != 0U) {
        output << " session_id=" << context.session_id;
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

LogContext context_from_frame(
    const std::string& service_name,
    const runtime::protocol::FrameMessage& frame) {
    LogContext context;
    context.service_name = service_name;
    context.request_id = frame.request_id();
    context.route_key = frame.route_key();
    context.message_id = frame.message_id();
    context.gateway_id = service_name;
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

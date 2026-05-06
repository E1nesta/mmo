#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "runtime/protocol/frame.h"

namespace runtime::observability {

struct LogContext {
    LogContext() = default;
    explicit LogContext(std::string service_name_value)
        : service_name(std::move(service_name_value)) {}

    std::string service_name;
    std::uint64_t request_id{};
    std::uint64_t route_key{};
    std::uint32_t message_id{};
    std::string gateway_id;
    std::uint64_t session_id{};
    std::string upstream;
    std::string status;
    int error_code{};
    std::int64_t latency_ms{-1};
};

LogContext context_from_frame(
    const std::string& service_name,
    const runtime::protocol::FrameMessage& frame);

std::string format_log_line(const LogContext& context, const std::string& event);

void log_info(const LogContext& context, const std::string& event);
void log_warn(const LogContext& context, const std::string& event);
void log_error(const LogContext& context, const std::string& event);

}  // namespace runtime::observability

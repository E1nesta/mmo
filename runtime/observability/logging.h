#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "common/envelope.pb.h"

namespace mmo::runtime::observability {

struct LogContext {
    LogContext() = default;
    explicit LogContext(std::string service_name_value)
        : service_name(std::move(service_name_value)) {}

    std::string service_name;
    std::uint64_t request_id{};
    std::int64_t account_id{};
    std::int64_t player_id{};
    std::string message_type;
    std::string trace_id;
    std::string gateway_id;
    std::string game_session_id;
    std::string upstream;
    std::string status;
    int error_code{};
    std::int64_t latency_ms{-1};
};

LogContext context_from_envelope(
    const std::string& service_name,
    const mmo::common::Envelope& envelope);

std::string format_log_line(const LogContext& context, const std::string& event);

void log_info(const LogContext& context, const std::string& event);
void log_warn(const LogContext& context, const std::string& event);
void log_error(const LogContext& context, const std::string& event);

}  // namespace mmo::runtime::observability

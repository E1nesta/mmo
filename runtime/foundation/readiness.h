#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "runtime/foundation/server_config.h"

namespace mmo::runtime::foundation {

struct ReadinessResult {
    bool ready{};
    std::string error_code;
    std::string message;
};

ReadinessResult make_ready_result();

ReadinessResult make_not_ready_result(
    std::string error_code,
    std::string message);

ReadinessResult check_tcp_dependency(
    const std::string& dependency_name,
    const std::string& host,
    std::uint16_t port,
    std::chrono::milliseconds timeout);

ReadinessResult check_tcp_dependency(
    const std::string& dependency_name,
    const ServiceConfig& service,
    std::chrono::milliseconds timeout);

}  // namespace mmo::runtime::foundation

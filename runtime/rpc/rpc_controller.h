#pragma once

#include <string>

namespace mmo::runtime::rpc {

struct RpcController {
    std::string source_service;
    std::string target_service;
    std::string trace_id;
    int connect_timeout_millis{};
    int request_timeout_millis{};
    bool retry_enabled{};
};

}  // namespace mmo::runtime::rpc

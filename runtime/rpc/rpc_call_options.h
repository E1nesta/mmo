#pragma once

#include <cstdint>
#include <string>

#include "runtime/rpc/rpc_routing_policy.h"

namespace runtime::rpc {

struct RpcCallOptions {
    std::string source_service;
    std::string target_service;
    RpcRoutingPolicy routing_policy{RpcRoutingPolicy::kUnspecified};
    std::uint64_t route_key{};
    std::string target_instance_id;
    int connect_timeout_millis{};
    int request_timeout_millis{};
    bool retry_enabled{};
};

}  // namespace runtime::rpc

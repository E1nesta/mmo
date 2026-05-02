#pragma once

#include <string>

#include "runtime/channel/routing_policy.h"

namespace mmo::runtime::rpc {

struct RpcController {
    std::string source_service;
    std::string target_service;
    std::string trace_id;
    mmo::runtime::channel::RoutingPolicy routing_policy{
        mmo::runtime::channel::RoutingPolicy::kUnspecified};
    std::string route_key;
    std::string target_instance_id;
    int connect_timeout_millis{};
    int request_timeout_millis{};
    bool retry_enabled{};
};

}  // namespace mmo::runtime::rpc

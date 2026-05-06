#pragma once

#include <cstdint>
#include <string>

#include "runtime/rpc/rpc_routing_policy.h"
#include "runtime/protocol/message_mode.h"

namespace runtime::rpc {

struct RpcOptions {
    std::string source_service;
    std::string target_service;
    runtime::rpc::RpcRoutingPolicy routing_policy{
        runtime::rpc::RpcRoutingPolicy::kUnspecified};
    std::uint64_t request_id{};
    std::uint64_t route_key{};
    std::string target_instance_id;
    int connect_timeout_millis{};
    int request_timeout_millis{};
    bool retry_enabled{};
    runtime::protocol::MessageMode mode{runtime::protocol::MessageMode::kCall};
};

}  // namespace runtime::rpc

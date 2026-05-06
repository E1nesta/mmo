#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/rpc/rpc_routing_policy.h"
#include "runtime/protocol/message_mode.h"

namespace runtime::gateway {

struct GatewayRoute {
    std::uint32_t public_message_id{};
    std::string target_service;
    std::uint32_t internal_message_id{};
    std::uint32_t public_response_message_id{};
    runtime::protocol::MessageMode mode{runtime::protocol::MessageMode::kCall};
    bool require_session{true};
    runtime::rpc::RpcRoutingPolicy routing_policy{
        runtime::rpc::RpcRoutingPolicy::kUnspecified};
    std::string route_key_source;
    std::string target_instance_id;
};

class GatewayRouteTable {
public:
    void add(std::uint32_t public_message_id, GatewayRoute target);
    std::optional<GatewayRoute> find(std::uint32_t public_message_id) const;
    bool contains(std::uint32_t public_message_id) const;
    std::vector<GatewayRoute> routes() const;
    std::size_t size() const;

private:
    std::unordered_map<std::uint32_t, GatewayRoute> routes_;
};

}  // namespace runtime::gateway

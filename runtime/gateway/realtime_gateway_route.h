#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/protocol/message_mode.h"
#include "runtime/rpc/rpc_routing_policy.h"

namespace runtime::gateway {

enum class RealtimeRouteKeyPolicy {
    kRealtimeSessionId = 1,
    kPlayerId = 2,
    kNoRouteKey = 3,
};

struct RealtimeGatewayRoute {
    std::uint16_t public_message_id{};
    std::string target_service;
    std::uint32_t target_message_id{};
    runtime::protocol::MessageMode mode{runtime::protocol::MessageMode::kCast};
    runtime::rpc::RpcRoutingPolicy routing_policy{
        runtime::rpc::RpcRoutingPolicy::kStickyRouteKey};
    RealtimeRouteKeyPolicy route_key_policy{RealtimeRouteKeyPolicy::kPlayerId};
    std::string target_instance_id;
};

class RealtimeGatewayRouteTable {
public:
    void add(std::uint16_t public_message_id, RealtimeGatewayRoute route);
    std::optional<RealtimeGatewayRoute> find(
        std::uint16_t public_message_id) const;
    bool contains(std::uint16_t public_message_id) const;
    std::vector<RealtimeGatewayRoute> routes() const;
    std::size_t size() const;

private:
    std::unordered_map<std::uint16_t, RealtimeGatewayRoute> routes_;
};

}  // namespace runtime::gateway

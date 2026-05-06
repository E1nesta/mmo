#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/rpc/rpc_error.h"
#include "runtime/rpc/rpc_service_registry.h"

namespace runtime::rpc {

enum class RpcRoutingPolicy {
    kUnspecified = 0,
    kRoundRobin = 1,
    kLeastPending = 2,
    kStickyRouteKey = 3,
    kExplicitInstance = 4,
};

struct RpcRouteSelectionContext {
    std::string target_service;
    RpcRoutingPolicy policy{RpcRoutingPolicy::kUnspecified};
    std::uint64_t request_id{};
    std::uint64_t route_key{};
    std::string target_instance_id;
    std::size_t max_pending_requests_per_instance{};
};

struct RpcRouteSelectionResult {
    RpcServiceInstance instance;
    RpcError error;

    bool ok() const;
};

class RpcServiceInstanceSelector {
public:
    RpcRouteSelectionResult select(
        const std::vector<RpcServiceInstance>& instances,
        const RpcRouteSelectionContext& context);

private:
    std::size_t next_round_robin_index(const std::string& service_name);

    std::mutex mutex_;
    std::unordered_map<std::string, std::size_t> round_robin_counters_;
};

std::string rpc_routing_policy_name(RpcRoutingPolicy policy);

}  // namespace runtime::rpc

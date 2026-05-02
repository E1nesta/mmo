#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/channel/channel_error.h"
#include "runtime/channel/service_registry.h"

namespace mmo::runtime::channel {

enum class RoutingPolicy {
    kUnspecified = 0,
    kRoundRobin = 1,
    kLeastPending = 2,
    kStickyPlayer = 3,
    kStickyInstance = 4,
    kExplicitInstance = 5,
};

struct RouteSelectionContext {
    std::string target_service;
    RoutingPolicy policy{RoutingPolicy::kUnspecified};
    std::int64_t player_id{};
    std::uint64_t request_id{};
    std::string message_type;
    std::string route_key;
    std::string target_instance_id;
    std::size_t max_pending_requests_per_instance{};
};

struct RouteSelectionResult {
    ServiceInstance instance;
    ChannelError error;

    bool ok() const;
};

class ServiceInstanceSelector {
public:
    RouteSelectionResult select(
        const std::vector<ServiceInstance>& instances,
        const RouteSelectionContext& context);

private:
    std::size_t next_round_robin_index(const std::string& service_name);

    std::mutex mutex_;
    std::unordered_map<std::string, std::size_t> round_robin_counters_;
};

std::string routing_policy_name(RoutingPolicy policy);

}  // namespace mmo::runtime::channel

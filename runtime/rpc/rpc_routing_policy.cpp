#include "runtime/rpc/rpc_routing_policy.h"

#include <algorithm>
#include <limits>

namespace runtime::rpc {
namespace {

std::uint64_t stable_hash(std::uint64_t value) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (int shift = 0; shift < 64; shift += 8) {
        hash ^= static_cast<unsigned char>((value >> shift) & 0xffULL);
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool is_eligible(
    const RpcServiceInstance& instance,
    const RpcRouteSelectionContext& context) {
    if (instance.state != RpcServiceInstanceState::kHealthy) {
        return false;
    }
    if (context.max_pending_requests_per_instance > 0 &&
        instance.pending_count >= context.max_pending_requests_per_instance) {
        return false;
    }
    return true;
}

}  // namespace

bool RpcRouteSelectionResult::ok() const {
    return error.ok();
}

RpcRouteSelectionResult RpcServiceInstanceSelector::select(
    const std::vector<RpcServiceInstance>& instances,
    const RpcRouteSelectionContext& context) {
    std::vector<RpcServiceInstance> eligible;
    eligible.reserve(instances.size());
    bool had_healthy_instance = false;
    for (const auto& instance : instances) {
        if (instance.state == RpcServiceInstanceState::kHealthy) {
            had_healthy_instance = true;
        }
        if (is_eligible(instance, context)) {
            eligible.push_back(instance);
        }
    }

    if (eligible.empty()) {
        if (had_healthy_instance && context.max_pending_requests_per_instance > 0) {
            return RpcRouteSelectionResult{
                {},
                make_rpc_error(
                    RpcErrorCode::kPendingLimitExceeded,
                    "rpc upstream instance pending limit exceeded")};
        }
        return RpcRouteSelectionResult{
            {},
            make_rpc_error(
                RpcErrorCode::kEndpointNotFound,
                "no healthy upstream instance: " + context.target_service)};
    }

    const auto policy = context.policy == RpcRoutingPolicy::kUnspecified
        ? (context.route_key > 0 ? RpcRoutingPolicy::kStickyRouteKey
                                 : RpcRoutingPolicy::kRoundRobin)
        : context.policy;

    if (policy == RpcRoutingPolicy::kExplicitInstance) {
        for (const auto& instance : eligible) {
            if (instance.instance_id == context.target_instance_id) {
                return RpcRouteSelectionResult{instance, RpcError{}};
            }
        }
        return RpcRouteSelectionResult{
            {},
            make_rpc_error(
                RpcErrorCode::kEndpointNotFound,
                "requested upstream instance is unavailable: " +
                    context.target_service + "/" + context.target_instance_id)};
    }

    if (policy == RpcRoutingPolicy::kLeastPending) {
        const auto it = std::min_element(
            eligible.begin(),
            eligible.end(),
            [](const RpcServiceInstance& lhs, const RpcServiceInstance& rhs) {
                if (lhs.pending_count == rhs.pending_count) {
                    return lhs.instance_id < rhs.instance_id;
                }
                return lhs.pending_count < rhs.pending_count;
            });
        return RpcRouteSelectionResult{*it, RpcError{}};
    }

    if (policy == RpcRoutingPolicy::kStickyRouteKey) {
        const auto key = context.route_key != 0 ? context.route_key
                                                : context.request_id;
        return RpcRouteSelectionResult{
            eligible[stable_hash(key) % eligible.size()], RpcError{}};
    }

    const auto index =
        next_round_robin_index(context.target_service) % eligible.size();
    return RpcRouteSelectionResult{eligible[index], RpcError{}};
}

std::size_t RpcServiceInstanceSelector::next_round_robin_index(
    const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& counter = round_robin_counters_[service_name];
    const auto index = counter;
    ++counter;
    return index;
}

std::string rpc_routing_policy_name(RpcRoutingPolicy policy) {
    switch (policy) {
        case RpcRoutingPolicy::kUnspecified:
            return "unspecified";
        case RpcRoutingPolicy::kRoundRobin:
            return "round_robin";
        case RpcRoutingPolicy::kLeastPending:
            return "least_pending";
        case RpcRoutingPolicy::kStickyRouteKey:
            return "sticky_route_key";
        case RpcRoutingPolicy::kExplicitInstance:
            return "explicit_instance";
    }
    return "unknown";
}

}  // namespace runtime::rpc

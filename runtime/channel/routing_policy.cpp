#include "runtime/channel/routing_policy.h"

#include <algorithm>
#include <limits>

namespace mmo::runtime::channel {
namespace {

std::uint64_t stable_hash(const std::string& value) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto ch : value) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool is_eligible(
    const ServiceInstance& instance,
    const RouteSelectionContext& context) {
    if (instance.state != ServiceInstanceState::kHealthy) {
        return false;
    }
    if (context.max_pending_requests_per_instance > 0 &&
        instance.pending_count >= context.max_pending_requests_per_instance) {
        return false;
    }
    return true;
}

}  // namespace

bool RouteSelectionResult::ok() const {
    return error.ok();
}

RouteSelectionResult ServiceInstanceSelector::select(
    const std::vector<ServiceInstance>& instances,
    const RouteSelectionContext& context) {
    std::vector<ServiceInstance> eligible;
    eligible.reserve(instances.size());
    bool had_healthy_instance = false;
    for (const auto& instance : instances) {
        if (instance.state == ServiceInstanceState::kHealthy) {
            had_healthy_instance = true;
        }
        if (is_eligible(instance, context)) {
            eligible.push_back(instance);
        }
    }

    if (eligible.empty()) {
        if (had_healthy_instance && context.max_pending_requests_per_instance > 0) {
            return RouteSelectionResult{
                {},
                make_channel_error(
                    ChannelErrorCode::kPendingLimitExceeded,
                    "channel upstream instance pending limit exceeded")};
        }
        return RouteSelectionResult{
            {},
            make_channel_error(
                ChannelErrorCode::kEndpointNotFound,
                "no healthy upstream instance: " + context.target_service)};
    }

    const auto policy = context.policy == RoutingPolicy::kUnspecified
        ? (context.player_id > 0 ? RoutingPolicy::kStickyPlayer
                                 : RoutingPolicy::kRoundRobin)
        : context.policy;

    if (policy == RoutingPolicy::kExplicitInstance) {
        for (const auto& instance : eligible) {
            if (instance.instance_id == context.target_instance_id) {
                return RouteSelectionResult{instance, ChannelError{}};
            }
        }
        return RouteSelectionResult{
            {},
            make_channel_error(
                ChannelErrorCode::kEndpointNotFound,
                "requested upstream instance is unavailable: " +
                    context.target_service + "/" + context.target_instance_id)};
    }

    if (policy == RoutingPolicy::kLeastPending) {
        const auto it = std::min_element(
            eligible.begin(),
            eligible.end(),
            [](const ServiceInstance& lhs, const ServiceInstance& rhs) {
                if (lhs.pending_count == rhs.pending_count) {
                    return lhs.instance_id < rhs.instance_id;
                }
                return lhs.pending_count < rhs.pending_count;
            });
        return RouteSelectionResult{*it, ChannelError{}};
    }

    if (policy == RoutingPolicy::kStickyPlayer) {
        const std::string key = context.player_id > 0
            ? std::to_string(context.player_id)
            : std::to_string(context.request_id);
        return RouteSelectionResult{
            eligible[stable_hash(key) % eligible.size()], ChannelError{}};
    }

    if (policy == RoutingPolicy::kStickyInstance) {
        const std::string key = !context.route_key.empty()
            ? context.route_key
            : context.target_instance_id;
        return RouteSelectionResult{
            eligible[stable_hash(key) % eligible.size()], ChannelError{}};
    }

    const auto index =
        next_round_robin_index(context.target_service) % eligible.size();
    return RouteSelectionResult{eligible[index], ChannelError{}};
}

std::size_t ServiceInstanceSelector::next_round_robin_index(
    const std::string& service_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& counter = round_robin_counters_[service_name];
    const auto index = counter;
    ++counter;
    return index;
}

std::string routing_policy_name(RoutingPolicy policy) {
    switch (policy) {
        case RoutingPolicy::kUnspecified:
            return "unspecified";
        case RoutingPolicy::kRoundRobin:
            return "round_robin";
        case RoutingPolicy::kLeastPending:
            return "least_pending";
        case RoutingPolicy::kStickyPlayer:
            return "sticky_player";
        case RoutingPolicy::kStickyInstance:
            return "sticky_instance";
        case RoutingPolicy::kExplicitInstance:
            return "explicit_instance";
    }
    return "unknown";
}

}  // namespace mmo::runtime::channel

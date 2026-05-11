#include "runtime/gateway/realtime_gateway_route.h"

#include <stdexcept>
#include <utility>

#include "runtime/protocol/frame.h"

namespace runtime::gateway {

void RealtimeGatewayRouteTable::add(
    std::uint16_t public_message_id,
    RealtimeGatewayRoute route) {
    if (public_message_id == 0U) {
        throw std::runtime_error(
            "realtime gateway route public message id must be non-zero");
    }
    if (routes_.find(public_message_id) != routes_.end()) {
        throw std::runtime_error(
            "duplicate realtime gateway route public message id");
    }
    if (route.target_service.empty()) {
        throw std::runtime_error(
            "realtime gateway route target service must be non-empty");
    }
    if (route.target_message_id == 0U) {
        throw std::runtime_error(
            "realtime gateway route target message id must be non-zero");
    }
    if (!runtime::protocol::is_cast_like(route.mode)) {
        throw std::runtime_error(
            "realtime gateway route must use cast or batch mode");
    }
    switch (route.route_key_policy) {
        case RealtimeRouteKeyPolicy::kRealtimeSessionId:
        case RealtimeRouteKeyPolicy::kPlayerId:
        case RealtimeRouteKeyPolicy::kNoRouteKey:
            break;
        default:
            throw std::runtime_error(
                "realtime gateway route key policy is invalid");
    }
    route.public_message_id = public_message_id;
    routes_[public_message_id] = std::move(route);
}

std::optional<RealtimeGatewayRoute> RealtimeGatewayRouteTable::find(
    std::uint16_t public_message_id) const {
    const auto it = routes_.find(public_message_id);
    if (it == routes_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool RealtimeGatewayRouteTable::contains(std::uint16_t public_message_id) const {
    return routes_.find(public_message_id) != routes_.end();
}

std::vector<RealtimeGatewayRoute> RealtimeGatewayRouteTable::routes() const {
    std::vector<RealtimeGatewayRoute> result;
    result.reserve(routes_.size());
    for (const auto& [public_message_id, route] : routes_) {
        (void)public_message_id;
        result.push_back(route);
    }
    return result;
}

std::size_t RealtimeGatewayRouteTable::size() const {
    return routes_.size();
}

}  // namespace runtime::gateway

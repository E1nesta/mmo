#include "runtime/gateway/gateway_route_table.h"

#include <stdexcept>
#include <utility>

namespace runtime::gateway {

void GatewayRouteTable::add(std::uint16_t public_message_id, GatewayRoute target) {
    if (public_message_id == 0U) {
        throw std::runtime_error("gateway route public message id must be non-zero");
    }
    if (routes_.find(public_message_id) != routes_.end()) {
        throw std::runtime_error("duplicate gateway route public message id");
    }
    if (target.target_service.empty()) {
        throw std::runtime_error("gateway route target service must be non-empty");
    }
    if (target.target_message_id == 0U) {
        throw std::runtime_error("gateway route target message id must be non-zero");
    }
    if (!runtime::protocol::is_valid_message_mode(target.mode)) {
        throw std::runtime_error("gateway route mode is invalid");
    }
    switch (target.route_key_policy) {
        case GatewayRouteKeyPolicy::kSessionId:
        case GatewayRouteKeyPolicy::kRequestId:
        case GatewayRouteKeyPolicy::kNoRouteKey:
        case GatewayRouteKeyPolicy::kPlayerId:
            break;
        default:
            throw std::runtime_error("gateway route key policy is invalid");
    }
    target.public_message_id = public_message_id;
    routes_[public_message_id] = std::move(target);
}

std::optional<GatewayRoute> GatewayRouteTable::find(std::uint16_t public_message_id) const {
    const auto it = routes_.find(public_message_id);
    if (it == routes_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool GatewayRouteTable::contains(std::uint16_t public_message_id) const {
    return routes_.find(public_message_id) != routes_.end();
}

std::vector<GatewayRoute> GatewayRouteTable::routes() const {
    std::vector<GatewayRoute> result;
    result.reserve(routes_.size());
    for (const auto& [public_message_id, route] : routes_) {
        (void)public_message_id;
        result.push_back(route);
    }
    return result;
}

std::size_t GatewayRouteTable::size() const {
    return routes_.size();
}

}  // namespace runtime::gateway

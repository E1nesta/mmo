#include "runtime/gateway/gateway_route_table.h"

#include <utility>

namespace runtime::gateway {

void GatewayRouteTable::add(std::uint32_t public_message_id, GatewayRoute target) {
    target.public_message_id = public_message_id;
    routes_[public_message_id] = std::move(target);
}

std::optional<GatewayRoute> GatewayRouteTable::find(std::uint32_t public_message_id) const {
    const auto it = routes_.find(public_message_id);
    if (it == routes_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool GatewayRouteTable::contains(std::uint32_t public_message_id) const {
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

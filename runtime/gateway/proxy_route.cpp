#include "runtime/gateway/proxy_route.h"

#include <utility>

namespace mmo::runtime::gateway {

void ProxyRouteTable::add(std::string client_message_type, ProxyRoute target) {
    routes_[std::move(client_message_type)] = std::move(target);
}

std::optional<ProxyRoute> ProxyRouteTable::find(
    const std::string& client_message_type) const {
    const auto it = routes_.find(client_message_type);
    if (it == routes_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool ProxyRouteTable::contains(const std::string& client_message_type) const {
    return routes_.find(client_message_type) != routes_.end();
}

std::size_t ProxyRouteTable::size() const {
    return routes_.size();
}

}  // namespace mmo::runtime::gateway

#include "runtime/routing/route_table.h"

#include <utility>

namespace mmo::runtime::routing {

void RouteTable::add(std::string client_message_type, RouteTarget target) {
    routes_[std::move(client_message_type)] = std::move(target);
}

std::optional<RouteTarget> RouteTable::find(
    const std::string& client_message_type) const {
    const auto it = routes_.find(client_message_type);
    if (it == routes_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool RouteTable::contains(const std::string& client_message_type) const {
    return routes_.find(client_message_type) != routes_.end();
}

std::size_t RouteTable::size() const {
    return routes_.size();
}

}  // namespace mmo::runtime::routing

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

#include "runtime/channel/routing_policy.h"

namespace mmo::runtime::routing {

struct RouteTarget {
    std::string target_service;
    std::string target_message_type;
    bool require_session{true};
    mmo::runtime::channel::RoutingPolicy routing_policy{
        mmo::runtime::channel::RoutingPolicy::kUnspecified};
    std::string route_key_source;
    std::string target_instance_id;
};

class RouteTable {
public:
    void add(std::string client_message_type, RouteTarget target);
    std::optional<RouteTarget> find(const std::string& client_message_type) const;
    bool contains(const std::string& client_message_type) const;
    std::size_t size() const;

private:
    std::unordered_map<std::string, RouteTarget> routes_;
};

}  // namespace mmo::runtime::routing

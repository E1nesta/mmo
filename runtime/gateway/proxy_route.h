#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

#include "runtime/channel/routing_policy.h"

namespace runtime::gateway {

struct ProxyRoute {
    std::string target_service;
    std::string target_message_type;
    bool require_session{true};
    runtime::channel::RoutingPolicy routing_policy{
        runtime::channel::RoutingPolicy::kUnspecified};
    std::string route_key_source;
    std::string target_instance_id;
};

class ProxyRouteTable {
public:
    void add(std::string client_message_type, ProxyRoute target);
    std::optional<ProxyRoute> find(const std::string& client_message_type) const;
    bool contains(const std::string& client_message_type) const;
    std::size_t size() const;

private:
    std::unordered_map<std::string, ProxyRoute> routes_;
};

}  // namespace runtime::gateway

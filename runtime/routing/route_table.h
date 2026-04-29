#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

namespace mmo::runtime::routing {

struct RouteTarget {
    std::string target_service;
    std::string target_message_type;
    bool require_session{true};
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

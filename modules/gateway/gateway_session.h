#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace mmo::modules::gateway {

struct SessionBinding {
    std::int64_t player_id{};
    std::string session_token;
    std::uint64_t connection_id{};
};

class GatewaySessionRegistry {
public:
    SessionBinding bind(std::int64_t player_id, const std::string& session_token);
    bool is_bound(std::int64_t player_id, const std::string& session_token) const;

private:
    std::uint64_t next_connection_id_{1};
    std::unordered_map<std::int64_t, SessionBinding> bindings_;
};

}  // namespace mmo::modules::gateway

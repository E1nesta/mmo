#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mmo::runtime::session {

struct RequestContext {
    std::uint64_t request_id{};
    std::int64_t player_id{};
    std::string session_token;
    std::string message_type;
};

struct ConnectionBinding {
    std::uint64_t connection_id{};
    std::int64_t player_id{};
    std::string session_token;
    bool authenticated{};
};

class SessionRegistry {
public:
    ConnectionBinding bind(std::int64_t player_id, const std::string& session_token);
    bool is_bound(std::int64_t player_id, const std::string& session_token) const;
    void unbind(std::int64_t player_id);

private:
    mutable std::mutex mutex_;
    std::uint64_t next_connection_id_{1};
    std::unordered_map<std::int64_t, ConnectionBinding> bindings_;
};

}  // namespace mmo::runtime::session

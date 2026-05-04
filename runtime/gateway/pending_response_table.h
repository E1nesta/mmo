#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace runtime::gateway {

struct PendingResponse {
    std::uint64_t connection_id{};
    std::uint64_t client_request_id{};
    std::int64_t player_id{};
};

class PendingResponseTable {
public:
    bool bind(std::uint64_t upstream_request_id, PendingResponse pending);
    std::optional<PendingResponse> remove(std::uint64_t upstream_request_id);
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, PendingResponse> pending_;
};

}  // namespace runtime::gateway

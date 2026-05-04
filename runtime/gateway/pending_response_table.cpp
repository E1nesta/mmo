#include "runtime/gateway/pending_response_table.h"

#include <utility>

namespace runtime::gateway {

bool PendingResponseTable::bind(
    std::uint64_t upstream_request_id,
    PendingResponse pending) {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.emplace(upstream_request_id, std::move(pending)).second;
}

std::optional<PendingResponse> PendingResponseTable::remove(
    std::uint64_t upstream_request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = pending_.find(upstream_request_id);
    if (it == pending_.end()) {
        return std::nullopt;
    }

    auto pending = it->second;
    pending_.erase(it);
    return pending;
}

std::size_t PendingResponseTable::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}

}  // namespace runtime::gateway

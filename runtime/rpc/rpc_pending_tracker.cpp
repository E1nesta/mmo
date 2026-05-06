#include "runtime/rpc/rpc_pending_tracker.h"

#include <utility>

namespace runtime::rpc {

RpcPendingTracker::RpcPendingTracker(std::size_t max_pending_requests)
    : max_pending_requests_(max_pending_requests > 0 ? max_pending_requests : 1) {}

RpcError RpcPendingTracker::add(
    std::uint64_t request_id,
    std::shared_ptr<RpcPendingCall> pending_call) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_.size() >= max_pending_requests_) {
        return make_rpc_error(
            RpcErrorCode::kPendingLimitExceeded,
            "rpc pending limit exceeded");
    }
    if (pending_.find(request_id) != pending_.end()) {
        return make_rpc_error(
            RpcErrorCode::kDuplicateRequestId,
            "rpc request_id is already pending");
    }
    pending_[request_id] = std::move(pending_call);
    return {};
}

void RpcPendingTracker::complete(std::uint64_t request_id, RpcResult result) {
    std::shared_ptr<RpcPendingCall> pending_call;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = pending_.find(request_id);
        if (it == pending_.end()) {
            return;
        }
        pending_call = it->second;
        pending_.erase(it);
    }
    pending_call->promise.set_value(std::move(result));
}

void RpcPendingTracker::remove(std::uint64_t request_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.erase(request_id);
}

void RpcPendingTracker::fail_all(const RpcError& error) {
    std::unordered_map<std::uint64_t, std::shared_ptr<RpcPendingCall>> pending;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending.swap(pending_);
    }
    for (auto& [request_id, pending_call] : pending) {
        (void)request_id;
        pending_call->promise.set_value(RpcResult::failure(error));
    }
}

std::size_t RpcPendingTracker::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_.size();
}

}  // namespace runtime::rpc

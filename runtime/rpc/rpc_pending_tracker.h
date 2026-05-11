#pragma once

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "runtime/rpc/rpc_error.h"
#include "runtime/rpc/rpc_result.h"

namespace runtime::rpc {

using RpcResultHandler = std::function<void(RpcResult)>;

struct RpcPendingCall {
    std::promise<RpcResult> promise;
    RpcResultHandler handler;
    std::chrono::steady_clock::time_point deadline;
};

class RpcPendingTracker {
public:
    explicit RpcPendingTracker(std::size_t max_pending_requests);

    RpcError add(
        std::uint64_t request_id,
        std::shared_ptr<RpcPendingCall> pending_call);
    void complete(std::uint64_t request_id, RpcResult result);
    void remove(std::uint64_t request_id);
    std::size_t expire(
        std::chrono::steady_clock::time_point now,
        const RpcError& error);
    void fail_all(const RpcError& error);
    std::size_t size() const;

private:
    std::size_t max_pending_requests_{};
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, std::shared_ptr<RpcPendingCall>> pending_;
};

}  // namespace runtime::rpc

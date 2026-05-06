#pragma once

#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "runtime/rpc/rpc_error.h"
#include "runtime/rpc/rpc_result.h"

namespace runtime::rpc {

struct RpcPendingCall {
    std::promise<RpcResult> promise;
};

class RpcPendingTracker {
public:
    explicit RpcPendingTracker(std::size_t max_pending_requests);

    RpcError add(
        std::uint64_t request_id,
        std::shared_ptr<RpcPendingCall> pending_call);
    void complete(std::uint64_t request_id, RpcResult result);
    void remove(std::uint64_t request_id);
    void fail_all(const RpcError& error);
    std::size_t size() const;

private:
    std::size_t max_pending_requests_{};
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, std::shared_ptr<RpcPendingCall>> pending_;
};

}  // namespace runtime::rpc

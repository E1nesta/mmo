#include <cassert>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>

#include "runtime/protocol/frame.h"
#include "runtime/rpc/rpc_dispatcher.h"
#include "runtime/rpc/rpc_error.h"
#include "runtime/rpc/rpc_pending_tracker.h"
#include "runtime/rpc/rpc_result.h"

namespace {

runtime::protocol::FrameMessage make_frame(std::uint32_t message_id) {
    runtime::protocol::FrameMessage frame;
    frame.header.message_id = message_id;
    frame.header.request_id = 77;
    frame.header.route_key = 1198216;
    frame.payload = "request";
    return frame;
}

void verify_dispatcher_routes_by_message_id() {
    runtime::rpc::RpcDispatcher dispatcher;
    dispatcher.on(30100, [](const runtime::protocol::FrameMessage& request) {
        runtime::protocol::FrameMessage response;
        response.header.message_id = 30101;
        response.header.request_id = request.request_id();
        response.header.route_key = request.route_key();
        response.payload = "ok";
        return response;
    });

    const auto response = dispatcher.dispatch(make_frame(30100));
    assert(response.message_id() == 30101);
    assert(response.request_id() == 77);
    assert(response.route_key() == 1198216);
    assert(response.payload == "ok");

    const auto missing = dispatcher.dispatch(make_frame(39999));
    assert(missing.message_id() == runtime::protocol::kErrorResponseMessageId);
}

void verify_dispatcher_rejects_invalid_registration() {
    runtime::rpc::RpcDispatcher dispatcher;

    bool zero_rejected = false;
    try {
        dispatcher.on(0, [](const runtime::protocol::FrameMessage&) {
            return runtime::protocol::FrameMessage{};
        });
    } catch (const std::runtime_error&) {
        zero_rejected = true;
    }
    assert(zero_rejected);

    dispatcher.on(30100, [](const runtime::protocol::FrameMessage&) {
        return runtime::protocol::FrameMessage{};
    });

    bool duplicate_rejected = false;
    try {
        dispatcher.on(30100, [](const runtime::protocol::FrameMessage&) {
            return runtime::protocol::FrameMessage{};
        });
    } catch (const std::runtime_error&) {
        duplicate_rejected = true;
    }
    assert(duplicate_rejected);
}

void verify_pending_tracker_limit_and_complete() {
    runtime::rpc::RpcPendingTracker pending(1);

    auto first = std::make_shared<runtime::rpc::RpcPendingCall>();
    first->deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    auto first_future = first->promise.get_future();
    assert(pending.add(1, first).ok());

    auto second = std::make_shared<runtime::rpc::RpcPendingCall>();
    second->deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    const auto overflow = pending.add(2, second);
    assert(overflow.code == runtime::rpc::RpcErrorCode::kPendingLimitExceeded);

    pending.complete(1, runtime::rpc::RpcResult::success(make_frame(30101)));
    const auto result = first_future.get();
    assert(result.ok());
    assert(result.has_response());
    assert(result.response().message_id() == 30101);
    assert(pending.size() == 0);
}

void verify_pending_tracker_duplicate_and_timeout() {
    runtime::rpc::RpcPendingTracker pending(2);

    auto first = std::make_shared<runtime::rpc::RpcPendingCall>();
    first->deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    assert(pending.add(1, first).ok());

    auto duplicate = std::make_shared<runtime::rpc::RpcPendingCall>();
    duplicate->deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    const auto duplicate_error = pending.add(1, duplicate);
    assert(duplicate_error.code == runtime::rpc::RpcErrorCode::kDuplicateRequestId);

    auto expired = std::make_shared<runtime::rpc::RpcPendingCall>();
    expired->deadline = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    auto expired_future = expired->promise.get_future();
    assert(pending.add(2, expired).ok());

    const auto expired_count = pending.expire(
        std::chrono::steady_clock::now(),
        runtime::rpc::make_rpc_error(
            runtime::rpc::RpcErrorCode::kTimeout, "timeout"));
    assert(expired_count == 1);
    const auto result = expired_future.get();
    assert(!result.ok());
    assert(result.error().code == runtime::rpc::RpcErrorCode::kTimeout);
    assert(pending.size() == 1);

    pending.fail_all(runtime::rpc::make_rpc_error(
        runtime::rpc::RpcErrorCode::kConnectionClosed, "closed"));
    assert(pending.size() == 0);
}

}  // namespace

int main() {
    verify_dispatcher_routes_by_message_id();
    verify_dispatcher_rejects_invalid_registration();
    verify_pending_tracker_limit_and_complete();
    verify_pending_tracker_duplicate_and_timeout();
    return 0;
}

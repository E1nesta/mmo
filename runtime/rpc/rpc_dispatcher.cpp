#include "runtime/rpc/rpc_dispatcher.h"

#include <future>
#include <memory>
#include <stdexcept>
#include <utility>

#include "runtime/protocol/payload_utils.h"

namespace runtime::rpc {

void RpcDispatcher::on(std::uint32_t message_id, Handler handler) {
    on_async(
        message_id,
        [handler = std::move(handler)](
            const runtime::protocol::FrameMessage& frame,
            ReplyHandler reply) {
            reply(handler(frame));
        });
}

void RpcDispatcher::on_async(std::uint32_t message_id, AsyncHandler handler) {
    if (message_id == 0U) {
        throw std::runtime_error("rpc handler message id must be non-zero");
    }
    if (handlers_.find(message_id) != handlers_.end()) {
        throw std::runtime_error("duplicate rpc handler message id");
    }
    handlers_[message_id] = std::move(handler);
}

runtime::protocol::FrameMessage RpcDispatcher::dispatch(
    const runtime::protocol::FrameMessage& frame) const {
    auto completed = std::make_shared<std::promise<
        runtime::protocol::FrameMessage>>();
    auto response = completed->get_future();
    dispatch_async(
        frame,
        [completed](runtime::protocol::FrameMessage reply) {
            completed->set_value(std::move(reply));
        });
    return response.get();
}

void RpcDispatcher::dispatch_async(
    const runtime::protocol::FrameMessage& frame,
    ReplyHandler reply) const {
    const auto it = handlers_.find(frame.message_id());
    if (it == handlers_.end()) {
        reply(runtime::protocol::make_error_frame(
            frame, 404, "rpc handler not found"));
        return;
    }
    it->second(frame, std::move(reply));
}

RpcDispatcher::AsyncHandler RpcDispatcher::handler() const {
    return [this](
        const runtime::protocol::FrameMessage& frame,
        ReplyHandler reply) {
        dispatch_async(frame, std::move(reply));
    };
}

}  // namespace runtime::rpc

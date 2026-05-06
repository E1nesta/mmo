#include "runtime/rpc/rpc_dispatcher.h"

#include <utility>

#include "runtime/protocol/payload_utils.h"

namespace runtime::rpc {

void RpcDispatcher::on(std::uint32_t message_id, Handler handler) {
    handlers_[message_id] = std::move(handler);
}

runtime::protocol::FrameMessage RpcDispatcher::dispatch(
    const runtime::protocol::FrameMessage& frame) const {
    const auto it = handlers_.find(frame.message_id());
    if (it == handlers_.end()) {
        return runtime::protocol::make_error_frame(
            frame, 404, "rpc handler not found");
    }
    return it->second(frame);
}

RpcDispatcher::Handler RpcDispatcher::handler() const {
    return [this](const runtime::protocol::FrameMessage& frame) {
        return dispatch(frame);
    };
}

}  // namespace runtime::rpc

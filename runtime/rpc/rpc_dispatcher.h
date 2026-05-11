#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "runtime/protocol/frame.h"

namespace runtime::rpc {

class RpcDispatcher {
public:
    using ReplyHandler = std::function<void(runtime::protocol::FrameMessage)>;
    using Handler = std::function<runtime::protocol::FrameMessage(
        const runtime::protocol::FrameMessage&)>;
    using AsyncHandler = std::function<void(
        const runtime::protocol::FrameMessage&,
        ReplyHandler)>;

    void on(std::uint32_t message_id, Handler handler);
    void on_async(std::uint32_t message_id, AsyncHandler handler);
    runtime::protocol::FrameMessage dispatch(
        const runtime::protocol::FrameMessage& frame) const;
    void dispatch_async(
        const runtime::protocol::FrameMessage& frame,
        ReplyHandler reply) const;
    AsyncHandler handler() const;

private:
    std::unordered_map<std::uint32_t, AsyncHandler> handlers_;
};

}  // namespace runtime::rpc

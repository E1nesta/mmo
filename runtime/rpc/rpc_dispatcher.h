#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "runtime/protocol/frame.h"

namespace runtime::rpc {

class RpcDispatcher {
public:
    using Handler = std::function<runtime::protocol::FrameMessage(
        const runtime::protocol::FrameMessage&)>;

    void on(std::uint32_t message_id, Handler handler);
    runtime::protocol::FrameMessage dispatch(
        const runtime::protocol::FrameMessage& frame) const;
    Handler handler() const;

private:
    std::unordered_map<std::uint32_t, Handler> handlers_;
};

}  // namespace runtime::rpc

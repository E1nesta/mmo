#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "runtime/net/frame_transport.h"
#include "runtime/protocol/frame.h"

namespace runtime::net {

using RpcFrameReplyHandler =
    std::function<void(runtime::protocol::FrameMessage)>;
using RpcFrameHandler = std::function<void(
    const runtime::protocol::FrameMessage&,
    RpcFrameReplyHandler)>;

class RpcFrameTransport final {
public:
    RpcFrameTransport(
        std::uint16_t port,
        RpcFrameHandler handler,
        std::string transport_name,
        TransportOptions options = {});

    int run();

private:
    std::uint16_t port_{};
    RpcFrameHandler handler_;
    std::string transport_name_;
    TransportOptions options_;
};

}  // namespace runtime::net

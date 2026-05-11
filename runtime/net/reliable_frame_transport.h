#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "runtime/net/frame_transport.h"
#include "runtime/net/reliable_frame_codec.h"

namespace runtime::net {

using ReliableFrameReplyHandler = std::function<void(ReliableFrame)>;
using ReliableFrameHandler = std::function<void(
    const ReliableFrame&,
    ReliableFrameReplyHandler)>;

class ReliableFrameTransport final {
public:
    ReliableFrameTransport(
        std::uint16_t port,
        ReliableFrameHandler handler,
        std::string transport_name,
        TransportOptions options = {});

    int run();

private:
    std::uint16_t port_{};
    ReliableFrameHandler handler_;
    std::string transport_name_;
    TransportOptions options_;
};

}  // namespace runtime::net

#pragma once

#include <cstdint>
#include <string>

#include "runtime/net/frame_transport.h"

namespace runtime::net {

class TcpFrameServer final : public FrameServer {
public:
    TcpFrameServer(
        std::uint16_t port,
        FrameHandler handler,
        std::string service_name,
        TransportOptions options = {});

    int run() override;

private:
    std::uint16_t port_{};
    FrameHandler handler_;
    std::string service_name_;
    TransportOptions options_;
};

}  // namespace runtime::net

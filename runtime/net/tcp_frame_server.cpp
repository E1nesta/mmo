#include "runtime/net/tcp_frame_server.h"

#include <utility>

namespace runtime::net {

TcpFrameServer::TcpFrameServer(
    std::uint16_t port,
    FrameHandler handler,
    std::string service_name,
    TransportOptions options)
    : port_(port),
      handler_(std::move(handler)),
      service_name_(std::move(service_name)),
      options_(options) {}

int TcpFrameServer::run() {
    (void)port_;
    (void)handler_;
    (void)service_name_;
    (void)options_;
    return 0;
}

}  // namespace runtime::net

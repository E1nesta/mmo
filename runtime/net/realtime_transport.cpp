#include "runtime/net/realtime_transport.h"

namespace runtime::net {

RealtimeTransport::RealtimeTransport(
    std::uint16_t port,
    KcpOptions options)
    : port_(port),
      options_(options) {}

std::uint16_t RealtimeTransport::port() const {
    return port_;
}

const KcpOptions& RealtimeTransport::options() const {
    return options_;
}

}  // namespace runtime::net

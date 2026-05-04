#include "runtime/transport/realtime_transport.h"

#include <algorithm>
#include <utility>

namespace runtime::transport {

RealtimeTransport::RealtimeTransport(
    std::uint16_t port,
    KcpOptions options,
    std::vector<std::string> accepted_message_types)
    : port_(port),
      options_(options),
      accepted_message_types_(std::move(accepted_message_types)) {}

std::uint16_t RealtimeTransport::port() const {
    return port_;
}

const KcpOptions& RealtimeTransport::options() const {
    return options_;
}

bool RealtimeTransport::accepts_message_type(const std::string& message_type) const {
    return std::find(
               accepted_message_types_.begin(),
               accepted_message_types_.end(),
               message_type) != accepted_message_types_.end();
}

}  // namespace runtime::transport

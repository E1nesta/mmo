#include "runtime/net/realtime_transport.h"

#include <algorithm>
#include <utility>

namespace runtime::net {

RealtimeTransport::RealtimeTransport(
    std::uint16_t port,
    KcpOptions options,
    std::vector<std::uint32_t> accepted_message_ids)
    : port_(port),
      options_(options),
      accepted_message_ids_(std::move(accepted_message_ids)) {}

std::uint16_t RealtimeTransport::port() const {
    return port_;
}

const KcpOptions& RealtimeTransport::options() const {
    return options_;
}

bool RealtimeTransport::accepts_message_id(std::uint32_t message_id) const {
    return std::find(
               accepted_message_ids_.begin(),
               accepted_message_ids_.end(),
               message_id) != accepted_message_ids_.end();
}

}  // namespace runtime::net

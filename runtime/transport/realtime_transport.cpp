#include "runtime/transport/realtime_transport.h"

#include "runtime/protocol/message_types.h"

namespace mmo::runtime::transport {

RealtimeTransport::RealtimeTransport(std::uint16_t port, KcpOptions options)
    : port_(port), options_(options) {}

std::uint16_t RealtimeTransport::port() const {
    return port_;
}

const KcpOptions& RealtimeTransport::options() const {
    return options_;
}

bool RealtimeTransport::accepts_message_type(const std::string& message_type) const {
    return message_type == mmo::runtime::protocol::kMoveCommand ||
           message_type == mmo::runtime::protocol::kCastSkillRequest;
}

}  // namespace mmo::runtime::transport

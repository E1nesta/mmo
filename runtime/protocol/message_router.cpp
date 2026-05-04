#include "runtime/protocol/message_router.h"

#include <utility>

#include "runtime/protocol/envelope_utils.h"

namespace runtime::protocol {

void MessageRouter::on(std::string message_type, Handler handler) {
    handlers_[std::move(message_type)] = std::move(handler);
}

mmo::common::Envelope MessageRouter::dispatch(
    const mmo::common::Envelope& envelope) const {
    const auto it = handlers_.find(envelope.message_type());
    if (it == handlers_.end()) {
        return make_error_envelope(envelope, 404, "unsupported message");
    }
    return it->second(envelope);
}

MessageRouter::Handler MessageRouter::handler() const {
    return [this](const mmo::common::Envelope& envelope) {
        return dispatch(envelope);
    };
}

}  // namespace runtime::protocol

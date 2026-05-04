#include "runtime/gateway/gateway_router.h"

#include <utility>

namespace runtime::gateway {

void GatewayRouter::on(std::string message_type, Handler handler) {
    router_.on(std::move(message_type), std::move(handler));
}

mmo::common::Envelope GatewayRouter::dispatch(
    const mmo::common::Envelope& envelope) const {
    return router_.dispatch(envelope);
}

GatewayRouter::Handler GatewayRouter::handler() const {
    return [this](const mmo::common::Envelope& envelope) {
        return dispatch(envelope);
    };
}

}  // namespace runtime::gateway

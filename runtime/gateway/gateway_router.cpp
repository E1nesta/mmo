#include "runtime/gateway/gateway_router.h"

#include <utility>

#include "runtime/gateway/gateway_middleware.h"

namespace runtime::gateway {

void GatewayRouter::on(std::uint32_t message_id, Handler handler) {
    handlers_[message_id] = std::move(handler);
}

runtime::net::ReliableFrame GatewayRouter::dispatch(
    const runtime::net::ReliableFrame& frame) const {
    const auto it = handlers_.find(frame.message_id);
    if (it == handlers_.end()) {
        return make_gateway_error_frame(frame, 404, "gateway handler not found");
    }
    return it->second(frame);
}

GatewayRouter::Handler GatewayRouter::handler() const {
    return [this](const runtime::net::ReliableFrame& frame) {
        return dispatch(frame);
    };
}

}  // namespace runtime::gateway

#include "runtime/gateway/gateway_router.h"

#include <future>
#include <memory>
#include <stdexcept>
#include <utility>

#include "runtime/gateway/gateway_middleware.h"

namespace runtime::gateway {

void GatewayRouter::on(std::uint16_t message_id, Handler handler) {
    if (message_id == 0U) {
        throw std::runtime_error("gateway handler message id must be non-zero");
    }
    if (handlers_.find(message_id) != handlers_.end()) {
        throw std::runtime_error("duplicate gateway handler message id");
    }
    handlers_[message_id] = std::move(handler);
}

void GatewayRouter::on_sync(std::uint16_t message_id, SyncHandler handler) {
    on(
        message_id,
        [handler = std::move(handler)](
            const runtime::net::ReliableFrame& frame,
            ReplyHandler reply) {
            reply(handler(frame));
        });
}

runtime::net::ReliableFrame GatewayRouter::dispatch(
    const runtime::net::ReliableFrame& frame) const {
    auto completed = std::make_shared<std::promise<runtime::net::ReliableFrame>>();
    auto future = completed->get_future();
    dispatch_async(
        frame,
        [completed](runtime::net::ReliableFrame response) {
            completed->set_value(std::move(response));
        });
    return future.get();
}

void GatewayRouter::dispatch_async(
    const runtime::net::ReliableFrame& frame,
    ReplyHandler reply) const {
    const auto it = handlers_.find(frame.message_id);
    if (it == handlers_.end()) {
        reply(make_gateway_error_frame(frame, 404, "gateway handler not found"));
        return;
    }
    it->second(frame, std::move(reply));
}

GatewayRouter::Handler GatewayRouter::handler() const {
    return [this](
        const runtime::net::ReliableFrame& frame,
        ReplyHandler reply) {
        dispatch_async(frame, std::move(reply));
    };
}

}  // namespace runtime::gateway

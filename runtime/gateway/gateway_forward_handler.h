#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "runtime/gateway/gateway_context.h"
#include "runtime/gateway/gateway_middleware.h"
#include "runtime/gateway/gateway_router.h"
#include "runtime/protocol/frame.h"
#include "runtime/rpc/rpc_error.h"

namespace runtime::gateway {

inline std::uint64_t gateway_route_key(
    const GatewayRoute& route,
    const runtime::net::ReliableFrame& frame) {
    if (route.route_key_source == "session_id") {
        return frame.session_id;
    }
    if (route.route_key_source == "request_id") {
        return frame.request_id;
    }
    return frame.session_id;
}

inline runtime::protocol::FrameMessage make_internal_frame(
    const GatewayRoute& route,
    const runtime::net::ReliableFrame& frame,
    std::uint64_t route_key) {
    runtime::protocol::FrameMessage internal;
    internal.header.message_id = route.internal_message_id;
    internal.header.request_id = frame.request_id;
    internal.header.route_key = route_key;
    internal.header.mode = route.mode;
    internal.header.flags = frame.flags;
    internal.payload.assign(frame.payload.begin(), frame.payload.end());
    return internal;
}

inline runtime::net::ReliableFrame make_public_frame(
    const runtime::net::ReliableFrame& request,
    std::uint32_t message_id,
    const runtime::protocol::FrameMessage& internal_response) {
    runtime::net::ReliableFrame response;
    response.version = request.version;
    response.flags = request.flags;
    response.message_id = message_id;
    response.request_id = request.request_id;
    response.session_id = request.session_id;
    response.payload.assign(
        internal_response.payload.begin(),
        internal_response.payload.end());
    return response;
}

inline void bind_gateway_forward_handler(
    GatewayRouter& router,
    std::uint32_t public_message_id,
    GatewayContext context) {
    router.on(
        public_message_id,
        [context](const runtime::net::ReliableFrame& frame) mutable {
            const auto route = context.route_table.find(frame.message_id);
            if (!route.has_value()) {
                return make_gateway_error_frame(
                    frame, 404, "gateway route is not configured");
            }

            if (route->require_session) {
                const auto validation_error = validate_gateway_session(
                    frame,
                    context.sessions,
                    context.session_store,
                    &context.security_metrics);
                if (validation_error.has_value()) {
                    return *validation_error;
                }
            }

            const auto route_key = gateway_route_key(*route, frame);
            auto internal_request = make_internal_frame(*route, frame, route_key);
            auto rpc_options = make_gateway_rpc_options(*route, route_key);

            const bool cast_like =
                runtime::protocol::is_cast_like(route->mode);
            const auto rpc_result = cast_like
                ? context.rpc_client.cast_frame(
                      route->target_service,
                      std::move(internal_request),
                      rpc_options)
                : context.rpc_client.call_frame(
                      route->target_service,
                      std::move(internal_request),
                      rpc_options);
            if (!rpc_result.ok()) {
                return make_gateway_error_frame(
                    frame,
                    runtime::rpc::rpc_error_to_status_code(rpc_result.error().code),
                    rpc_result.error().message);
            }

            if (!rpc_result.has_response()) {
                runtime::protocol::FrameMessage empty_response;
                empty_response.header.request_id = frame.request_id;
                empty_response.header.route_key = route_key;
                return make_public_frame(
                    frame,
                    route->public_response_message_id != 0U
                        ? route->public_response_message_id
                        : frame.message_id,
                    empty_response);
            }

            return make_public_frame(
                frame,
                route->public_response_message_id != 0U
                    ? route->public_response_message_id
                    : frame.message_id,
                rpc_result.response());
        });
}

}  // namespace runtime::gateway

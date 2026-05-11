#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "runtime/gateway/gateway_context.h"
#include "runtime/gateway/gateway_middleware.h"
#include "runtime/gateway/gateway_router.h"
#include "runtime/protocol/frame.h"
#include "runtime/protocol/payload_utils.h"
#include "runtime/rpc/rpc_error.h"

namespace runtime::gateway {

inline std::uint64_t gateway_route_key(
    const GatewayRoute& route,
    const runtime::net::ReliableFrame& frame,
    const runtime::session::SessionRegistry& sessions) {
    switch (route.route_key_policy) {
        case GatewayRouteKeyPolicy::kSessionId:
            return frame.session_id;
        case GatewayRouteKeyPolicy::kRequestId:
            return frame.request_id;
        case GatewayRouteKeyPolicy::kNoRouteKey:
            return 0;
        case GatewayRouteKeyPolicy::kPlayerId: {
            const auto binding = sessions.find_by_connection_id(frame.session_id);
            return binding.has_value() && binding->player_id > 0
                ? static_cast<std::uint64_t>(binding->player_id)
                : 0;
        }
    }
    return frame.session_id;
}

inline runtime::protocol::FrameMessage make_internal_frame(
    const GatewayRoute& route,
    const runtime::net::ReliableFrame& frame,
    std::uint64_t route_key) {
    runtime::protocol::FrameMessage internal;
    internal.header.message_id = route.target_message_id;
    internal.header.request_id = frame.request_id;
    internal.header.route_key = route_key;
    internal.header.mode = route.mode;
    internal.header.flags = frame.flags;
    internal.payload.assign(frame.payload.begin(), frame.payload.end());
    return internal;
}

inline runtime::net::ReliableFrame make_public_frame(
    const runtime::net::ReliableFrame& request,
    std::uint16_t message_id,
    const runtime::protocol::FrameMessage& internal_response) {
    runtime::net::ReliableFrame response;
    response.version = request.version;
    response.flags = request.flags;
    response.message_id = static_cast<std::uint16_t>(message_id);
    response.request_id = request.request_id;
    response.session_id = request.session_id;
    response.payload.assign(
        internal_response.payload.begin(),
        internal_response.payload.end());
    return response;
}

inline void bind_gateway_route_handler(
    GatewayRouter& router,
    std::uint16_t public_message_id,
    GatewayContext context) {
    router.on(
        public_message_id,
        [context](
            const runtime::net::ReliableFrame& frame,
            GatewayRouter::ReplyHandler reply) mutable {
            const auto route = context.route_table.find(frame.message_id);
            if (!route.has_value()) {
                reply(make_gateway_error_frame(
                    frame, 404, "gateway route is not configured"));
                return;
            }

            if (route->require_session) {
                const auto validation_error = validate_gateway_session(
                    frame,
                    context.sessions,
                    &context.security_metrics);
                if (validation_error.has_value()) {
                    reply(*validation_error);
                    return;
                }
            }

            const auto route_key =
                gateway_route_key(*route, frame, context.sessions);
            if (route->route_key_policy == GatewayRouteKeyPolicy::kPlayerId &&
                route_key == 0) {
                reply(make_gateway_error_frame(
                    frame, 401, "gateway player route key is unavailable"));
                return;
            }
            auto internal_request = make_internal_frame(*route, frame, route_key);
            auto rpc_options = make_gateway_rpc_options(*route, route_key);

            const bool cast_like =
                runtime::protocol::is_cast_like(route->mode);
            if (cast_like) {
                const auto rpc_result = context.rpc_client.cast_frame(
                    route->target_service,
                    std::move(internal_request),
                    rpc_options);
                if (!rpc_result.ok()) {
                    reply(make_gateway_error_frame(
                        frame,
                        runtime::rpc::rpc_error_to_status_code(
                            rpc_result.error().code),
                        rpc_result.error().message));
                    return;
                }
                runtime::protocol::FrameMessage empty_response;
                empty_response.header.request_id = frame.request_id;
                empty_response.header.route_key = route_key;
                reply(make_public_frame(
                    frame,
                    route->public_response_message_id != 0U
                        ? route->public_response_message_id
                        : frame.message_id,
                    empty_response));
                return;
            }

            const auto route_snapshot = *route;
            auto reply_once =
                std::make_shared<GatewayRouter::ReplyHandler>(std::move(reply));
            const auto async_error = context.rpc_client.call_frame_async(
                route_snapshot.target_service,
                std::move(internal_request),
                rpc_options,
                [frame,
                 route_snapshot,
                 route_key,
                 reply_once](
                    runtime::rpc::RpcResult rpc_result) mutable {
                if (!rpc_result.ok()) {
                    if (rpc_result.has_response() &&
                        rpc_result.response().message_id() ==
                            runtime::protocol::kErrorResponseMessageId) {
                        mmo::common::Result error_result;
                        if (runtime::protocol::parse_payload(
                                rpc_result.response(), &error_result)) {
                            (*reply_once)(make_gateway_error_frame(
                                frame,
                                error_result.error().code(),
                                error_result.error().message()));
                            return;
                        }
                    }
                    (*reply_once)(make_gateway_error_frame(
                        frame,
                        runtime::rpc::rpc_error_to_status_code(
                            rpc_result.error().code),
                        rpc_result.error().message));
                    return;
                }

                if (!rpc_result.has_response()) {
                    runtime::protocol::FrameMessage empty_response;
                    empty_response.header.request_id = frame.request_id;
                    empty_response.header.route_key = route_key;
                    (*reply_once)(make_public_frame(
                        frame,
                        route_snapshot.public_response_message_id != 0U
                            ? route_snapshot.public_response_message_id
                            : frame.message_id,
                        empty_response));
                    return;
                }

                (*reply_once)(make_public_frame(
                    frame,
                    route_snapshot.public_response_message_id != 0U
                        ? route_snapshot.public_response_message_id
                        : frame.message_id,
                    rpc_result.response()));
            });
            if (!async_error.ok()) {
                (*reply_once)(make_gateway_error_frame(
                    frame,
                    runtime::rpc::rpc_error_to_status_code(async_error.code),
                    async_error.message));
            }
        });
}

}  // namespace runtime::gateway

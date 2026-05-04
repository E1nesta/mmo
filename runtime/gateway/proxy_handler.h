#pragma once

#include <string>
#include <utility>

#include "common/envelope.pb.h"
#include "runtime/gateway/gateway_middleware.h"
#include "runtime/gateway/gateway_router.h"
#include "runtime/gateway/proxy_context.h"
#include "runtime/gateway/proxy_mapper.h"
#include "runtime/protocol/envelope_utils.h"

namespace mmo::runtime::gateway {

template <
    typename PublicRequest,
    typename InternalRequest,
    typename InternalResponse,
    typename PublicResponse>
void bind_proxy_handler(
    GatewayRouter& router,
    std::string public_message_type,
    ProxyContext context,
    ProxyMapper<
        PublicRequest,
        InternalRequest,
        InternalResponse,
        PublicResponse> mapper) {
    router.on(
        public_message_type,
        [context, mapper = std::move(mapper)](
            const mmo::common::Envelope& envelope) mutable {
            PublicRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, mapper.invalid_public_request_message);
            }

            const auto route = context.route_table.find(envelope.message_type());
            if (!route.has_value()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "gateway route is not configured");
            }

            if (route->require_session) {
                const auto validation_error = validate_gateway_session(
                    envelope,
                    context.sessions,
                    context.session_store,
                    request.context(),
                    &context.security_metrics);
                if (validation_error.has_value()) {
                    return *validation_error;
                }
            }

            const auto internal_request = mapper.map_request(request);
            std::string route_key;
            if (mapper.route_key) {
                route_key = mapper.route_key(request);
            }
            const auto proxy_result = context.forwarder.forward(
                route->target_service,
                route->target_message_type,
                request.context(),
                internal_request,
                make_proxy_call_options(*route, request.context(), route_key));
            if (!proxy_result.ok()) {
                return proxy_result.make_error_envelope(envelope);
            }

            InternalResponse internal_response;
            if (!mmo::runtime::protocol::unpack_message(
                    proxy_result.response(), internal_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, mapper.invalid_internal_response_message);
            }
            if (!internal_response.context().success()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    internal_response.context().error_code(),
                    internal_response.context().error_message());
            }

            const auto response = mapper.map_response(request, internal_response);
            return mmo::runtime::protocol::pack_message(
                mapper.public_response_message_type,
                request.context(),
                response);
        });
}

}  // namespace mmo::runtime::gateway

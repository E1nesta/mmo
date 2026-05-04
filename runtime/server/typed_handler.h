#pragma once

#include <string>
#include <type_traits>
#include <utility>

#include "common/envelope.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/server/error_mapper.h"
#include "runtime/server/handler_chain.h"
#include "runtime/server/handler_result.h"
#include "runtime/server/request_validator.h"
#include "runtime/server/service_context.h"

namespace runtime::server {

template <typename Request, typename Response, typename Handler>
void bind_typed_handler(
    runtime::rpc::RpcServer& rpc_server,
    std::string request_message_type,
    std::string response_message_type,
    std::string service_name,
    Handler handler,
    HandlerChain chain,
    RequestValidator<Request> validator) {
    rpc_server.on(
        request_message_type,
        [response_message_type = std::move(response_message_type),
         service_name = std::move(service_name),
         handler = std::move(handler),
         chain = std::move(chain),
         validator = std::move(validator)](
            const mmo::common::Envelope& envelope) mutable {
            Request request;
            if (!runtime::protocol::unpack_message(envelope, request)) {
                return runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid request payload");
            }

            const auto context =
                make_service_context(service_name, envelope, request.context());
            const auto middleware_result = chain.run(context);
            if (!middleware_result.ok()) {
                return runtime::protocol::make_error_envelope(
                    envelope,
                    middleware_result.error_code,
                    middleware_result.error_message);
            }

            const auto validation_result =
                run_request_validator(request, context, validator);
            if (!validation_result.ok()) {
                return runtime::protocol::make_error_envelope(
                    envelope,
                    validation_result.error_code,
                    validation_result.error_message);
            }

            using Result = decltype(handler(request, context));
            static_assert(
                std::is_same<Result, HandlerResult<Response>>::value,
                "typed handler must return HandlerResult<Response>");

            const auto result = handler(request, context);
            if (!result.ok()) {
                return make_error_envelope_from_result(envelope, result);
            }

            return runtime::protocol::pack_message(
                response_message_type, request.context(), result.response());
        });
}

template <typename Request, typename Response, typename Handler>
void bind_typed_handler(
    runtime::rpc::RpcServer& rpc_server,
    std::string request_message_type,
    std::string response_message_type,
    std::string service_name,
    Handler handler,
    HandlerChain chain = {}) {
    bind_typed_handler<Request, Response>(
        rpc_server,
        std::move(request_message_type),
        std::move(response_message_type),
        std::move(service_name),
        std::move(handler),
        std::move(chain),
        RequestValidator<Request>{});
}

}  // namespace runtime::server

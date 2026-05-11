#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "runtime/handler/error_mapper.h"
#include "runtime/handler/handler_chain.h"
#include "runtime/handler/handler_context.h"
#include "runtime/handler/handler_result.h"
#include "runtime/handler/request_validator.h"
#include "runtime/protocol/payload_utils.h"
#include "runtime/rpc/rpc_dispatcher.h"

namespace runtime::handler {

template <typename Response>
void reply_with_handler_result(
    const runtime::protocol::FrameMessage& frame,
    std::uint32_t response_message_id,
    const HandlerResult<Response>& result,
    runtime::rpc::RpcDispatcher::ReplyHandler reply) {
    if (!result.ok()) {
        reply(make_error_frame_from_result(frame, result));
        return;
    }

    reply(runtime::protocol::pack_message(
        response_message_id,
        frame.request_id(),
        frame.route_key(),
        runtime::protocol::MessageMode::kReply,
        result.response()));
}

template <typename Request, typename Response, typename Handler>
void bind_typed_handler(
    runtime::rpc::RpcDispatcher& dispatcher,
    std::uint32_t request_message_id,
    std::uint32_t response_message_id,
    std::string service_name,
    Handler handler,
    HandlerChain chain,
    RequestValidator<Request> validator) {
    dispatcher.on(
        request_message_id,
        [response_message_id,
         service_name = std::move(service_name),
         handler = std::move(handler),
         chain = std::move(chain),
         validator = std::move(validator)](
            const runtime::protocol::FrameMessage& frame) mutable {
            const auto context = make_handler_context(service_name, frame);
            const auto middleware_result = chain.run(context);
            if (!middleware_result.ok()) {
                return runtime::protocol::make_error_frame(
                    frame,
                    middleware_result.error_code(),
                    middleware_result.error_message());
            }

            Request request;
            if (!runtime::protocol::parse_payload(frame, &request)) {
                return runtime::protocol::make_error_frame(
                    frame, 400, "invalid request payload");
            }

            const auto validation_result =
                run_request_validator(request, context, validator);
            if (!validation_result.ok()) {
                return runtime::protocol::make_error_frame(
                    frame,
                    validation_result.error_code(),
                    validation_result.error_message());
            }

            using Result = decltype(handler(request, context));
            static_assert(
                std::is_same<Result, HandlerResult<Response>>::value,
                "typed handler must return HandlerResult<Response>");

            const auto result = handler(request, context);
            runtime::protocol::FrameMessage response;
            reply_with_handler_result(
                frame,
                response_message_id,
                result,
                [&response](runtime::protocol::FrameMessage frame_response) {
                    response = std::move(frame_response);
                });
            return response;
        });
}

template <typename Request, typename Response, typename Handler>
void bind_async_typed_handler(
    runtime::rpc::RpcDispatcher& dispatcher,
    std::uint32_t request_message_id,
    std::uint32_t response_message_id,
    std::string service_name,
    Handler handler,
    HandlerChain chain,
    RequestValidator<Request> validator) {
    dispatcher.on_async(
        request_message_id,
        [response_message_id,
         service_name = std::move(service_name),
         handler = std::move(handler),
         chain = std::move(chain),
         validator = std::move(validator)](
            const runtime::protocol::FrameMessage& frame,
            runtime::rpc::RpcDispatcher::ReplyHandler reply) mutable {
            const auto context = make_handler_context(service_name, frame);
            const auto middleware_result = chain.run(context);
            if (!middleware_result.ok()) {
                reply(runtime::protocol::make_error_frame(
                    frame,
                    middleware_result.error_code(),
                    middleware_result.error_message()));
                return;
            }

            Request request;
            if (!runtime::protocol::parse_payload(frame, &request)) {
                reply(runtime::protocol::make_error_frame(
                    frame, 400, "invalid request payload"));
                return;
            }

            const auto validation_result =
                run_request_validator(request, context, validator);
            if (!validation_result.ok()) {
                reply(runtime::protocol::make_error_frame(
                    frame,
                    validation_result.error_code(),
                    validation_result.error_message()));
                return;
            }

            handler(
                request,
                context,
                [frame,
                 response_message_id,
                 reply = std::move(reply)](
                    HandlerResult<Response> result) mutable {
                    reply_with_handler_result(
                        frame,
                        response_message_id,
                        result,
                        std::move(reply));
                });
        });
}

template <typename Request, typename Response, typename Handler>
void bind_async_typed_handler(
    runtime::rpc::RpcDispatcher& dispatcher,
    std::uint32_t request_message_id,
    std::uint32_t response_message_id,
    std::string service_name,
    Handler handler,
    HandlerChain chain = {}) {
    bind_async_typed_handler<Request, Response>(
        dispatcher,
        request_message_id,
        response_message_id,
        std::move(service_name),
        std::move(handler),
        std::move(chain),
        RequestValidator<Request>{});
}

template <typename Request, typename Response, typename Handler>
void bind_typed_handler(
    runtime::rpc::RpcDispatcher& dispatcher,
    std::uint32_t request_message_id,
    std::uint32_t response_message_id,
    std::string service_name,
    Handler handler,
    HandlerChain chain = {}) {
    bind_typed_handler<Request, Response>(
        dispatcher,
        request_message_id,
        response_message_id,
        std::move(service_name),
        std::move(handler),
        std::move(chain),
        RequestValidator<Request>{});
}

}  // namespace runtime::handler

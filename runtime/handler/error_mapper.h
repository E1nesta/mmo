#pragma once

#include "runtime/handler/handler_result.h"
#include "runtime/protocol/payload_utils.h"

namespace runtime::handler {

template <typename Response>
runtime::protocol::FrameMessage make_error_frame_from_result(
    const runtime::protocol::FrameMessage& request,
    const HandlerResult<Response>& result) {
    return runtime::protocol::make_error_frame(
        request,
        result.error_code(),
        result.error_message().empty() ? "handler failed" : result.error_message());
}

}  // namespace runtime::handler

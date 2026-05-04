#pragma once

#include "common/envelope.pb.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/server/handler_result.h"

namespace runtime::server {

template <typename Response>
mmo::common::Envelope make_error_envelope_from_result(
    const mmo::common::Envelope& request,
    const HandlerResult<Response>& result) {
    return runtime::protocol::make_error_envelope(
        request,
        result.error_code(),
        result.error_message().empty() ? "handler failed" : result.error_message());
}

}  // namespace runtime::server

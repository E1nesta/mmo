#pragma once

#include <functional>

#include "runtime/handler/middleware.h"
#include "runtime/handler/handler_context.h"

namespace runtime::handler {

template <typename Request>
using RequestValidator =
    std::function<MiddlewareResult(const Request&, const HandlerContext&)>;

template <typename Request>
MiddlewareResult run_request_validator(
    const Request& request,
    const HandlerContext& context,
    const RequestValidator<Request>& validator) {
    if (!validator) {
        return MiddlewareResult::allow();
    }
    return validator(request, context);
}

}  // namespace runtime::handler

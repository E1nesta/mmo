#pragma once

#include <functional>

#include "runtime/server/middleware.h"
#include "runtime/server/service_context.h"

namespace mmo::runtime::server {

template <typename Request>
using RequestValidator =
    std::function<MiddlewareResult(const Request&, const ServiceContext&)>;

template <typename Request>
MiddlewareResult run_request_validator(
    const Request& request,
    const ServiceContext& context,
    const RequestValidator<Request>& validator) {
    if (!validator) {
        return MiddlewareResult::allow();
    }
    return validator(request, context);
}

}  // namespace mmo::runtime::server

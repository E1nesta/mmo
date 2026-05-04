#pragma once

#include <functional>
#include <optional>
#include <string>

#include "runtime/server/service_context.h"

namespace mmo::runtime::server {

struct MiddlewareResult {
    static MiddlewareResult allow() {
        return MiddlewareResult{};
    }

    static MiddlewareResult reject(int code, std::string message) {
        MiddlewareResult result;
        result.error_code = code;
        result.error_message = std::move(message);
        return result;
    }

    bool ok() const {
        return error_code == 0;
    }

    int error_code{};
    std::string error_message;
};

using Middleware = std::function<MiddlewareResult(const ServiceContext&)>;

}  // namespace mmo::runtime::server

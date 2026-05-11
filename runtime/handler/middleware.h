#pragma once

#include <functional>
#include <optional>
#include <string>

#include "runtime/handler/handler_context.h"

namespace runtime::handler {

struct MiddlewareResult {
    static MiddlewareResult allow() {
        return MiddlewareResult{};
    }

    static MiddlewareResult reject(int code, std::string message) {
        MiddlewareResult result;
        result.error_code_ = code;
        result.error_message_ = std::move(message);
        return result;
    }

    bool ok() const {
        return error_code_ == 0;
    }

    int error_code() const {
        return error_code_;
    }

    const std::string& error_message() const {
        return error_message_;
    }

private:
    int error_code_{};
    std::string error_message_;
};

using Middleware = std::function<MiddlewareResult(const HandlerContext&)>;

}  // namespace runtime::handler

#pragma once

#include <utility>
#include <vector>

#include "runtime/handler/middleware.h"

namespace runtime::handler {

class HandlerChain {
public:
    HandlerChain() = default;

    explicit HandlerChain(std::vector<Middleware> middleware)
        : middleware_(std::move(middleware)) {}

    void use(Middleware middleware) {
        middleware_.push_back(std::move(middleware));
    }

    MiddlewareResult run(const HandlerContext& context) const {
        for (const auto& middleware : middleware_) {
            const auto result = middleware(context);
            if (!result.ok()) {
                return result;
            }
        }
        return MiddlewareResult::allow();
    }

private:
    std::vector<Middleware> middleware_;
};

}  // namespace runtime::handler

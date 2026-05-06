#pragma once

#include <string>
#include <utility>

namespace runtime::handler {

template <typename Response>
class HandlerResult {
public:
    static HandlerResult success(Response response) {
        HandlerResult result;
        result.response_ = std::move(response);
        result.ok_ = true;
        return result;
    }

    static HandlerResult failure(int error_code, std::string error_message) {
        HandlerResult result;
        result.error_code_ = error_code;
        result.error_message_ = std::move(error_message);
        result.ok_ = false;
        return result;
    }

    bool ok() const {
        return ok_;
    }

    const Response& response() const {
        return response_;
    }

    Response& response() {
        return response_;
    }

    int error_code() const {
        return error_code_;
    }

    const std::string& error_message() const {
        return error_message_;
    }

private:
    Response response_;
    int error_code_{500};
    std::string error_message_{"handler failed"};
    bool ok_{false};
};

}  // namespace runtime::handler

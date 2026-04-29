#include "runtime/routing/forward_result.h"

#include <utility>

#include "runtime/protocol/envelope_utils.h"

namespace mmo::runtime::routing {

ForwardResult ForwardResult::success(mmo::common::Envelope response) {
    ForwardResult result;
    result.response_ = std::move(response);
    result.has_response_ = true;
    return result;
}

ForwardResult ForwardResult::failure(
    mmo::runtime::channel::ChannelError error) {
    ForwardResult result;
    result.error_ = std::move(error);
    return result;
}

ForwardResult ForwardResult::remote_error(mmo::common::Envelope response) {
    ForwardResult result;
    result.response_ = std::move(response);
    result.error_ = mmo::runtime::channel::make_channel_error(
        mmo::runtime::channel::ChannelErrorCode::kRemoteError,
        "remote forward error");
    result.has_response_ = true;
    return result;
}

bool ForwardResult::ok() const {
    return error_.ok();
}

bool ForwardResult::has_response() const {
    return has_response_;
}

const mmo::common::Envelope& ForwardResult::response() const {
    return response_;
}

const mmo::runtime::channel::ChannelError& ForwardResult::error() const {
    return error_;
}

mmo::common::Envelope ForwardResult::make_error_envelope(
    const mmo::common::Envelope& request) const {
    if (!ok() && has_response_) {
        return response_;
    }
    const auto status_code =
        mmo::runtime::channel::channel_error_to_status_code(error_.code);
    const auto message = error_.message.empty() ? "forward failed" : error_.message;
    return mmo::runtime::protocol::make_error_envelope(
        request, status_code, message);
}

}  // namespace mmo::runtime::routing

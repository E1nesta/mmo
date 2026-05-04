#include "runtime/channel/channel_result.h"

#include <utility>

#include "runtime/protocol/envelope_utils.h"

namespace runtime::channel {

ChannelResult ChannelResult::success(mmo::common::Envelope response) {
    ChannelResult result;
    result.response_ = std::move(response);
    result.has_response_ = true;
    return result;
}

ChannelResult ChannelResult::failure(ChannelError error) {
    ChannelResult result;
    result.error_ = std::move(error);
    return result;
}

ChannelResult ChannelResult::remote_error(mmo::common::Envelope response) {
    ChannelResult result;
    result.response_ = std::move(response);
    result.error_ =
        make_channel_error(ChannelErrorCode::kRemoteError, "remote channel error");
    result.has_response_ = true;
    return result;
}

bool ChannelResult::ok() const {
    return error_.ok();
}

bool ChannelResult::has_response() const {
    return has_response_;
}

const mmo::common::Envelope& ChannelResult::response() const {
    return response_;
}

const ChannelError& ChannelResult::error() const {
    return error_;
}

mmo::common::Envelope ChannelResult::make_error_envelope(
    const mmo::common::Envelope& request) const {
    const auto status_code = channel_error_to_status_code(error_.code);
    const auto message =
        error_.message.empty() ? "channel failed" : error_.message;
    return runtime::protocol::make_error_envelope(
        request, status_code, message);
}

}  // namespace runtime::channel

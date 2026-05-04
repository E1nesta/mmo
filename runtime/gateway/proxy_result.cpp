#include "runtime/gateway/proxy_result.h"

#include <utility>

#include "runtime/protocol/envelope_utils.h"

namespace mmo::runtime::gateway {

ProxyResult ProxyResult::success(mmo::common::Envelope response) {
    ProxyResult result;
    result.response_ = std::move(response);
    result.has_response_ = true;
    return result;
}

ProxyResult ProxyResult::failure(
    mmo::runtime::channel::ChannelError error) {
    ProxyResult result;
    result.error_ = std::move(error);
    return result;
}

ProxyResult ProxyResult::remote_error(mmo::common::Envelope response) {
    ProxyResult result;
    result.response_ = std::move(response);
    result.error_ = mmo::runtime::channel::make_channel_error(
        mmo::runtime::channel::ChannelErrorCode::kRemoteError,
        "remote forward error");
    result.has_response_ = true;
    return result;
}

bool ProxyResult::ok() const {
    return error_.ok();
}

bool ProxyResult::has_response() const {
    return has_response_;
}

const mmo::common::Envelope& ProxyResult::response() const {
    return response_;
}

const mmo::runtime::channel::ChannelError& ProxyResult::error() const {
    return error_;
}

mmo::common::Envelope ProxyResult::make_error_envelope(
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

}  // namespace mmo::runtime::gateway

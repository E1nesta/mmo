#include "runtime/rpc/rpc_result.h"

#include <utility>

#include "runtime/protocol/envelope_utils.h"

namespace mmo::runtime::rpc {

RpcResult RpcResult::success(mmo::common::Envelope response) {
    RpcResult result;
    result.response_ = std::move(response);
    result.has_response_ = true;
    return result;
}

RpcResult RpcResult::failure(RpcError error) {
    RpcResult result;
    result.error_ = std::move(error);
    return result;
}

RpcResult RpcResult::remote_error(mmo::common::Envelope response) {
    RpcResult result;
    result.response_ = std::move(response);
    result.error_ = make_rpc_error(RpcErrorCode::kRemoteError, "remote rpc error");
    result.has_response_ = true;
    return result;
}

bool RpcResult::ok() const {
    return error_.ok();
}

bool RpcResult::has_response() const {
    return has_response_;
}

const mmo::common::Envelope& RpcResult::response() const {
    return response_;
}

const RpcError& RpcResult::error() const {
    return error_;
}

mmo::common::Envelope RpcResult::make_error_envelope(
    const mmo::common::Envelope& request) const {
    const auto status_code = rpc_error_to_status_code(error_.code);
    const auto message = error_.message.empty() ? "rpc failed" : error_.message;
    return mmo::runtime::protocol::make_error_envelope(
        request, status_code, message);
}

}  // namespace mmo::runtime::rpc

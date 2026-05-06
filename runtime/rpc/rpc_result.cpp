#include "runtime/rpc/rpc_result.h"

#include <utility>

#include "runtime/protocol/payload_utils.h"

namespace runtime::rpc {

RpcResult RpcResult::success(runtime::protocol::FrameMessage response) {
    RpcResult result;
    result.response_ = std::move(response);
    result.has_response_ = true;
    return result;
}

RpcResult RpcResult::accepted() {
    RpcResult result;
    result.has_response_ = false;
    return result;
}

RpcResult RpcResult::failure(RpcError error) {
    RpcResult result;
    result.error_ = std::move(error);
    return result;
}

RpcResult RpcResult::remote_error(runtime::protocol::FrameMessage response) {
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

const runtime::protocol::FrameMessage& RpcResult::response() const {
    return response_;
}

const RpcError& RpcResult::error() const {
    return error_;
}

runtime::protocol::FrameMessage RpcResult::make_error_frame(
    const runtime::protocol::FrameMessage& request) const {
    if (!ok() && has_response_) {
        return response_;
    }
    const auto status_code = rpc_error_to_status_code(error_.code);
    const auto message = error_.message.empty() ? "rpc failed" : error_.message;
    return runtime::protocol::make_error_frame(request, status_code, message);
}

}  // namespace runtime::rpc

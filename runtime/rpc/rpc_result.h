#pragma once

#include "runtime/protocol/frame.h"
#include "runtime/rpc/rpc_error.h"

namespace runtime::rpc {

class RpcResult {
public:
    static RpcResult success(runtime::protocol::FrameMessage response);
    static RpcResult accepted();
    static RpcResult failure(RpcError error);
    static RpcResult remote_error(runtime::protocol::FrameMessage response);

    bool ok() const;
    bool has_response() const;
    const runtime::protocol::FrameMessage& response() const;
    const RpcError& error() const;

    runtime::protocol::FrameMessage make_error_frame(
        const runtime::protocol::FrameMessage& request) const;

private:
    runtime::protocol::FrameMessage response_;
    RpcError error_;
    bool has_response_{};
};

}  // namespace runtime::rpc

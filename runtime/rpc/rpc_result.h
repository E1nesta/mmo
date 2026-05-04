#pragma once

#include "common/envelope.pb.h"
#include "runtime/rpc/rpc_error.h"

namespace runtime::rpc {

class RpcResult {
public:
    static RpcResult success(mmo::common::Envelope response);
    static RpcResult failure(RpcError error);
    static RpcResult remote_error(mmo::common::Envelope response);

    bool ok() const;
    bool has_response() const;
    const mmo::common::Envelope& response() const;
    const RpcError& error() const;

    mmo::common::Envelope make_error_envelope(
        const mmo::common::Envelope& request) const;

private:
    mmo::common::Envelope response_;
    RpcError error_;
    bool has_response_{};
};

}  // namespace runtime::rpc

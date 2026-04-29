#include "runtime/rpc/rpc_error.h"

#include <utility>

namespace mmo::runtime::rpc {

bool RpcError::ok() const {
    return code == RpcErrorCode::kOk;
}

RpcError make_rpc_error(RpcErrorCode code, std::string message) {
    RpcError error;
    error.code = code;
    error.message = std::move(message);
    return error;
}

int rpc_error_to_status_code(RpcErrorCode code) {
    switch (code) {
        case RpcErrorCode::kOk:
            return 0;
        case RpcErrorCode::kEndpointNotFound:
            return 404;
        case RpcErrorCode::kTimeout:
            return 504;
        case RpcErrorCode::kPendingLimitExceeded:
        case RpcErrorCode::kDuplicateRequestId:
            return 429;
        case RpcErrorCode::kRemoteError:
            return 502;
        case RpcErrorCode::kConnectFailed:
        case RpcErrorCode::kWriteFailed:
        case RpcErrorCode::kReadFailed:
        case RpcErrorCode::kEncodeFailed:
        case RpcErrorCode::kDecodeFailed:
        case RpcErrorCode::kRequestIdMismatch:
        case RpcErrorCode::kChannelClosed:
            return 502;
    }
    return 500;
}

}  // namespace mmo::runtime::rpc

#pragma once

#include <string>

namespace runtime::rpc {

enum class RpcErrorCode {
    kOk = 0,
    kEndpointNotFound = 1,
    kConnectFailed = 2,
    kWriteFailed = 3,
    kReadFailed = 4,
    kEncodeFailed = 5,
    kDecodeFailed = 6,
    kTimeout = 7,
    kRemoteError = 8,
    kRequestIdMismatch = 9,
    kConnectionClosed = 10,
    kPendingLimitExceeded = 11,
    kDuplicateRequestId = 12,
};

struct RpcError {
    RpcErrorCode code{RpcErrorCode::kOk};
    std::string message;

    bool ok() const;
};

RpcError make_rpc_error(RpcErrorCode code, std::string message);
int rpc_error_to_status_code(RpcErrorCode code);

}  // namespace runtime::rpc

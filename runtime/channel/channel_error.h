#pragma once

#include <string>

namespace mmo::runtime::channel {

enum class ChannelErrorCode {
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
    kChannelClosed = 10,
    kPendingLimitExceeded = 11,
    kDuplicateRequestId = 12,
};

struct ChannelError {
    ChannelErrorCode code{ChannelErrorCode::kOk};
    std::string message;

    bool ok() const;
};

ChannelError make_channel_error(ChannelErrorCode code, std::string message);
int channel_error_to_status_code(ChannelErrorCode code);

}  // namespace mmo::runtime::channel

#include "runtime/channel/channel_error.h"

#include <utility>

namespace mmo::runtime::channel {

bool ChannelError::ok() const {
    return code == ChannelErrorCode::kOk;
}

ChannelError make_channel_error(ChannelErrorCode code, std::string message) {
    ChannelError error;
    error.code = code;
    error.message = std::move(message);
    return error;
}

int channel_error_to_status_code(ChannelErrorCode code) {
    switch (code) {
        case ChannelErrorCode::kOk:
            return 0;
        case ChannelErrorCode::kEndpointNotFound:
            return 404;
        case ChannelErrorCode::kTimeout:
            return 504;
        case ChannelErrorCode::kPendingLimitExceeded:
        case ChannelErrorCode::kDuplicateRequestId:
            return 429;
        case ChannelErrorCode::kRemoteError:
            return 502;
        case ChannelErrorCode::kConnectFailed:
        case ChannelErrorCode::kWriteFailed:
        case ChannelErrorCode::kReadFailed:
        case ChannelErrorCode::kEncodeFailed:
        case ChannelErrorCode::kDecodeFailed:
        case ChannelErrorCode::kRequestIdMismatch:
        case ChannelErrorCode::kChannelClosed:
            return 502;
    }
    return 500;
}

}  // namespace mmo::runtime::channel

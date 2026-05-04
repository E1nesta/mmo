#pragma once

#include "common/envelope.pb.h"
#include "runtime/channel/channel_error.h"

namespace runtime::channel {

class ChannelResult {
public:
    static ChannelResult success(mmo::common::Envelope response);
    static ChannelResult failure(ChannelError error);
    static ChannelResult remote_error(mmo::common::Envelope response);

    bool ok() const;
    bool has_response() const;
    const mmo::common::Envelope& response() const;
    const ChannelError& error() const;

    mmo::common::Envelope make_error_envelope(
        const mmo::common::Envelope& request) const;

private:
    mmo::common::Envelope response_;
    ChannelError error_;
    bool has_response_{};
};

}  // namespace runtime::channel

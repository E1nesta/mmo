#pragma once

#include "common/envelope.pb.h"
#include "runtime/channel/channel_error.h"

namespace mmo::runtime::gateway {

class ProxyResult {
public:
    static ProxyResult success(mmo::common::Envelope response);
    static ProxyResult failure(mmo::runtime::channel::ChannelError error);
    static ProxyResult remote_error(mmo::common::Envelope response);

    bool ok() const;
    bool has_response() const;
    const mmo::common::Envelope& response() const;
    const mmo::runtime::channel::ChannelError& error() const;

    mmo::common::Envelope make_error_envelope(
        const mmo::common::Envelope& request) const;

private:
    mmo::common::Envelope response_;
    mmo::runtime::channel::ChannelError error_;
    bool has_response_{};
};

}  // namespace mmo::runtime::gateway

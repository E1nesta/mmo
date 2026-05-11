#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "runtime/net/reliable_frame_codec.h"

namespace runtime::gateway {

class GatewayRouter {
public:
    using ReplyHandler = std::function<void(runtime::net::ReliableFrame)>;
    using SyncHandler = std::function<runtime::net::ReliableFrame(
        const runtime::net::ReliableFrame&)>;
    using Handler = std::function<void(
        const runtime::net::ReliableFrame&,
        ReplyHandler)>;

    void on(std::uint16_t message_id, Handler handler);
    void on_sync(std::uint16_t message_id, SyncHandler handler);
    runtime::net::ReliableFrame dispatch(
        const runtime::net::ReliableFrame& frame) const;
    void dispatch_async(
        const runtime::net::ReliableFrame& frame,
        ReplyHandler reply) const;
    Handler handler() const;

private:
    std::unordered_map<std::uint16_t, Handler> handlers_;
};

}  // namespace runtime::gateway

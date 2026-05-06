#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

#include "runtime/net/reliable_frame_codec.h"

namespace runtime::gateway {

class GatewayRouter {
public:
    using Handler = std::function<runtime::net::ReliableFrame(
        const runtime::net::ReliableFrame&)>;

    void on(std::uint32_t message_id, Handler handler);
    runtime::net::ReliableFrame dispatch(
        const runtime::net::ReliableFrame& frame) const;
    Handler handler() const;

private:
    std::unordered_map<std::uint32_t, Handler> handlers_;
};

}  // namespace runtime::gateway

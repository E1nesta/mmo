#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "runtime/net/frame_transport.h"
#include "runtime/net/tcp_session.h"
#include "runtime/scheduler/io_context_pool.h"

namespace runtime::net {

class TcpListener {
public:
    using SessionHandler = std::function<void(std::shared_ptr<TcpSession>)>;

    TcpListener(
        std::uint16_t port,
        SessionHandler handler,
        std::string listener_name,
        TransportOptions options = {});

    int run();
    TransportStats stats() const;

private:
    std::uint16_t port_{};
    SessionHandler handler_;
    std::string listener_name_;
    TransportOptions options_;
    std::shared_ptr<TransportCounters> counters_;
    runtime::scheduler::IOContextPool io_context_pool_;
};

}  // namespace runtime::net

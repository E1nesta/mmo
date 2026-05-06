#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "runtime/foundation/server_config.h"
#include "runtime/protocol/frame.h"

namespace runtime::net {

inline constexpr std::uint32_t kDefaultMaxFramePayloadBytes = 1024 * 1024;
inline constexpr int kDefaultTransportTimeoutMillis = 3000;

struct TransportOptions {
    std::uint32_t max_payload_bytes{kDefaultMaxFramePayloadBytes};
    int timeout_millis{kDefaultTransportTimeoutMillis};
    int listen_backlog{64};
    int io_thread_count{1};
    int handler_shard_count{1};
    std::size_t max_handler_queue_depth_per_shard{1024};
};

struct TransportEndpoint {
    std::string host;
    std::uint16_t port{};
};

TransportOptions make_transport_options(
    const runtime::foundation::TcpTransportConfig& config);

TransportOptions make_transport_options(
    const runtime::foundation::TcpTransportConfig& config,
    const runtime::foundation::SchedulerConfig& scheduler_config);

TransportEndpoint make_transport_endpoint(
    const runtime::foundation::ServiceConfig& config);

using FrameHandler = std::function<runtime::protocol::FrameMessage(
    const runtime::protocol::FrameMessage&)>;

class FrameServer {
public:
    virtual ~FrameServer() = default;

    virtual int run() = 0;
};

class FrameClient {
public:
    virtual ~FrameClient() = default;

    virtual runtime::protocol::FrameMessage send(
        const TransportEndpoint& endpoint,
        const runtime::protocol::FrameMessage& request) = 0;
};

}  // namespace runtime::net

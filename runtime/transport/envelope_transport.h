#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "common/envelope.pb.h"
#include "runtime/foundation/server_config.h"

namespace mmo::runtime::transport {

inline constexpr std::uint32_t kDefaultMaxEnvelopePayloadBytes = 1024 * 1024;
inline constexpr int kDefaultTransportTimeoutMillis = 3000;

struct TransportOptions {
    std::uint32_t max_payload_bytes{kDefaultMaxEnvelopePayloadBytes};
    int timeout_millis{kDefaultTransportTimeoutMillis};
    int listen_backlog{64};
};

struct TransportEndpoint {
    std::string host;
    std::uint16_t port{};
};

TransportOptions make_transport_options(
    const mmo::runtime::foundation::TcpTransportConfig& config);

TransportEndpoint make_transport_endpoint(
    const mmo::runtime::foundation::ServiceConfig& config);

using EnvelopeHandler =
    std::function<mmo::common::Envelope(const mmo::common::Envelope&)>;

class EnvelopeServer {
public:
    virtual ~EnvelopeServer() = default;

    virtual int run() = 0;
};

class EnvelopeClient {
public:
    virtual ~EnvelopeClient() = default;

    virtual mmo::common::Envelope send(
        const TransportEndpoint& endpoint,
        const mmo::common::Envelope& request) = 0;
};

}  // namespace mmo::runtime::transport

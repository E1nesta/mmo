#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "public/common.pb.h"
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

using EnvelopeHandler =
    std::function<mmo::public_api::Envelope(const mmo::public_api::Envelope&)>;

class EnvelopeServer {
public:
    virtual ~EnvelopeServer() = default;

    virtual int run() = 0;
};

class EnvelopeClient {
public:
    virtual ~EnvelopeClient() = default;

    virtual mmo::public_api::Envelope send(
        const TransportEndpoint& endpoint,
        const mmo::public_api::Envelope& request) = 0;
};

}  // namespace mmo::runtime::transport

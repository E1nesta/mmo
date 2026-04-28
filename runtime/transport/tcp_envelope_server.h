#pragma once

#include <cstdint>
#include <string>

#include "runtime/transport/envelope_transport.h"

namespace mmo::runtime::transport {

class TcpEnvelopeServer : public EnvelopeServer {
public:
    TcpEnvelopeServer(
        std::uint16_t port,
        EnvelopeHandler handler,
        std::string service_name,
        TransportOptions options = {});

    int run() override;

private:
    std::uint16_t port_{};
    EnvelopeHandler handler_;
    std::string service_name_;
    TransportOptions options_;
};

mmo::public_api::Envelope send_envelope(
    const std::string& host,
    std::uint16_t port,
    const mmo::public_api::Envelope& request);

mmo::public_api::Envelope send_envelope(
    const std::string& host,
    std::uint16_t port,
    const mmo::public_api::Envelope& request,
    const TransportOptions& options);

}  // namespace mmo::runtime::transport

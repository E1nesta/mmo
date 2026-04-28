#pragma once

#include "runtime/transport/envelope_transport.h"

namespace mmo::runtime::transport {

class TcpEnvelopeClient : public EnvelopeClient {
public:
    explicit TcpEnvelopeClient(TransportOptions options = {});

    mmo::common::Envelope send(
        const TransportEndpoint& endpoint,
        const mmo::common::Envelope& request) override;

private:
    TransportOptions options_;
};

}  // namespace mmo::runtime::transport

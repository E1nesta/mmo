#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "public/common.pb.h"

namespace mmo::runtime::transport {

using EnvelopeHandler =
    std::function<mmo::public_api::Envelope(const mmo::public_api::Envelope&)>;

class TcpEnvelopeServer {
public:
    TcpEnvelopeServer(std::uint16_t port, EnvelopeHandler handler);

    int run() const;

private:
    std::uint16_t port_{};
    EnvelopeHandler handler_;
};

mmo::public_api::Envelope send_envelope(
    const std::string& host,
    std::uint16_t port,
    const mmo::public_api::Envelope& request);

}  // namespace mmo::runtime::transport

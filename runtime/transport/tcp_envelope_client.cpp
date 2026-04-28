#include "runtime/transport/tcp_envelope_client.h"

#include <boost/asio.hpp>

#include <arpa/inet.h>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include "runtime/protocol/envelope_utils.h"

namespace mmo::runtime::transport {
namespace {

using boost::asio::ip::tcp;

bool read_exact(std::istream& input, void* buffer, std::size_t size) {
    input.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
    return input.good() || input.gcount() == static_cast<std::streamsize>(size);
}

bool write_exact(std::ostream& output, const void* buffer, std::size_t size) {
    output.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(size));
    output.flush();
    return static_cast<bool>(output);
}

bool read_envelope(
    std::istream& input,
    mmo::common::Envelope& envelope,
    std::uint32_t max_payload_bytes) {
    std::uint32_t network_size = 0;
    if (!read_exact(input, &network_size, sizeof(network_size))) {
        return false;
    }

    const std::uint32_t payload_size = ntohl(network_size);
    if (payload_size > max_payload_bytes) {
        return false;
    }

    std::string payload(payload_size, '\0');
    if (payload_size > 0 &&
        !read_exact(input, payload.data(), static_cast<std::size_t>(payload_size))) {
        return false;
    }

    return envelope.ParseFromString(payload);
}

bool write_envelope(
    std::ostream& output,
    const mmo::common::Envelope& envelope,
    std::uint32_t max_payload_bytes) {
    std::string payload;
    envelope.SerializeToString(&payload);
    if (payload.size() > max_payload_bytes) {
        return false;
    }

    const std::uint32_t network_size =
        htonl(static_cast<std::uint32_t>(payload.size()));
    return write_exact(output, &network_size, sizeof(network_size)) &&
           write_exact(output, payload.data(), payload.size());
}

}  // namespace

TcpEnvelopeClient::TcpEnvelopeClient(TransportOptions options)
    : options_(options) {}

mmo::common::Envelope TcpEnvelopeClient::send(
    const TransportEndpoint& endpoint,
    const mmo::common::Envelope& request) {
    try {
        tcp::iostream stream;
        stream.expires_after(std::chrono::milliseconds(options_.timeout_millis));
        stream.connect(endpoint.host, std::to_string(endpoint.port));
        if (!stream) {
            return mmo::runtime::protocol::make_error_envelope(
                request, 502, "failed to connect upstream");
        }

        if (!write_envelope(stream, request, options_.max_payload_bytes)) {
            return mmo::runtime::protocol::make_error_envelope(
                request, 502, "failed to send upstream request");
        }

        mmo::common::Envelope response;
        if (!read_envelope(stream, response, options_.max_payload_bytes)) {
            return mmo::runtime::protocol::make_error_envelope(
                request, 502, "failed to read upstream response");
        }

        return response;
    } catch (const std::exception& error) {
        return mmo::runtime::protocol::make_error_envelope(
            request, 502, std::string("transport exception: ") + error.what());
    }
}

}  // namespace mmo::runtime::transport

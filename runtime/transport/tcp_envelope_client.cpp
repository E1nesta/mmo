#include "runtime/transport/tcp_envelope_client.h"

#include <boost/asio.hpp>

#include <array>
#include <chrono>
#include <iostream>
#include <string>

#include "runtime/protocol/envelope_utils.h"
#include "runtime/transport/envelope_codec.h"

namespace runtime::transport {
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
    std::array<char, EnvelopeCodec::kHeaderBytes> header{};
    if (!read_exact(input, header.data(), header.size())) {
        return false;
    }

    std::uint32_t payload_size = 0;
    std::string error_message;
    if (!EnvelopeCodec::decode_payload_size(
            header, max_payload_bytes, &payload_size, &error_message)) {
        return false;
    }

    std::string payload(payload_size, '\0');
    if (payload_size > 0 &&
        !read_exact(input, payload.data(), static_cast<std::size_t>(payload_size))) {
        return false;
    }

    return EnvelopeCodec::parse_payload(payload, &envelope, &error_message);
}

bool write_envelope(
    std::ostream& output,
    const mmo::common::Envelope& envelope,
    std::uint32_t max_payload_bytes) {
    std::string payload;
    std::string error_message;
    if (!EnvelopeCodec::serialize_payload(
            envelope, max_payload_bytes, &payload, &error_message)) {
        return false;
    }

    const auto header = EnvelopeCodec::encode_payload_size(
        static_cast<std::uint32_t>(payload.size()));
    return write_exact(output, header.data(), header.size()) &&
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
            return runtime::protocol::make_error_envelope(
                request, 502, "failed to connect upstream");
        }

        if (!write_envelope(stream, request, options_.max_payload_bytes)) {
            return runtime::protocol::make_error_envelope(
                request, 502, "failed to send upstream request");
        }

        mmo::common::Envelope response;
        if (!read_envelope(stream, response, options_.max_payload_bytes)) {
            return runtime::protocol::make_error_envelope(
                request, 502, "failed to read upstream response");
        }

        return response;
    } catch (const std::exception& error) {
        return runtime::protocol::make_error_envelope(
            request, 502, std::string("transport exception: ") + error.what());
    }
}

}  // namespace runtime::transport

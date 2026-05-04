#include "runtime/transport/envelope_codec.h"

#include <limits>

namespace runtime::transport {

std::array<char, EnvelopeCodec::kHeaderBytes> EnvelopeCodec::encode_payload_size(
    std::uint32_t size) {
    return {
        static_cast<char>((size >> 24) & 0xFF),
        static_cast<char>((size >> 16) & 0xFF),
        static_cast<char>((size >> 8) & 0xFF),
        static_cast<char>(size & 0xFF),
    };
}

bool EnvelopeCodec::decode_payload_size(
    const std::array<char, kHeaderBytes>& header,
    std::uint32_t max_payload_bytes,
    std::uint32_t* payload_size,
    std::string* error_message) {
    if (payload_size == nullptr) {
        if (error_message != nullptr) {
            *error_message = "payload size output is null";
        }
        return false;
    }

    const auto size =
        (static_cast<std::uint32_t>(static_cast<unsigned char>(header[0])) << 24) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(header[1])) << 16) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(header[2])) << 8) |
        static_cast<std::uint32_t>(static_cast<unsigned char>(header[3]));
    if (size > max_payload_bytes) {
        if (error_message != nullptr) {
            *error_message = "envelope payload exceeds max size";
        }
        return false;
    }

    *payload_size = size;
    return true;
}

bool EnvelopeCodec::serialize_payload(
    const mmo::common::Envelope& envelope,
    std::uint32_t max_payload_bytes,
    std::string* payload,
    std::string* error_message) {
    if (payload == nullptr) {
        if (error_message != nullptr) {
            *error_message = "payload output is null";
        }
        return false;
    }
    if (envelope.ByteSizeLong() >
        static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max())) {
        if (error_message != nullptr) {
            *error_message = "envelope payload exceeds uint32 frame size";
        }
        return false;
    }

    payload->clear();
    if (!envelope.SerializeToString(payload)) {
        if (error_message != nullptr) {
            *error_message = "failed to serialize envelope";
        }
        return false;
    }
    if (payload->size() > max_payload_bytes) {
        if (error_message != nullptr) {
            *error_message = "envelope payload exceeds max size";
        }
        return false;
    }
    return true;
}

bool EnvelopeCodec::parse_payload(
    const std::string& payload,
    mmo::common::Envelope* envelope,
    std::string* error_message) {
    if (envelope == nullptr) {
        if (error_message != nullptr) {
            *error_message = "envelope output is null";
        }
        return false;
    }
    if (!envelope->ParseFromString(payload)) {
        if (error_message != nullptr) {
            *error_message = "failed to parse envelope payload";
        }
        return false;
    }
    return true;
}

}  // namespace runtime::transport

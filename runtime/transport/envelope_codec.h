#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "common/envelope.pb.h"

namespace runtime::transport {

class EnvelopeCodec {
public:
    static constexpr std::size_t kHeaderBytes = 4;

    static std::array<char, kHeaderBytes> encode_payload_size(std::uint32_t size);

    static bool decode_payload_size(
        const std::array<char, kHeaderBytes>& header,
        std::uint32_t max_payload_bytes,
        std::uint32_t* payload_size,
        std::string* error_message);

    static bool serialize_payload(
        const mmo::common::Envelope& envelope,
        std::uint32_t max_payload_bytes,
        std::string* payload,
        std::string* error_message);

    static bool parse_payload(
        const std::string& payload,
        mmo::common::Envelope* envelope,
        std::string* error_message);
};

}  // namespace runtime::transport

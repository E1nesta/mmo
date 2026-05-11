#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace runtime::net {

struct ReliableFrame {
    std::uint8_t version{};
    std::uint16_t message_id{};
    std::uint16_t flags{};
    std::uint64_t request_id{};
    std::uint64_t session_id{};
    std::vector<std::uint8_t> payload;
};

class ReliableFrameCodec {
public:
    static constexpr std::uint32_t kFrameLengthBytes = 4;
    static constexpr std::uint32_t kReliableFrameOverheadBytes =
        sizeof(std::uint8_t) +   // version
        sizeof(std::uint16_t) +  // message_id
        sizeof(std::uint16_t) +  // flags
        sizeof(std::uint64_t) +  // request_id
        sizeof(std::uint64_t) +  // session_id
        sizeof(std::uint32_t);   // payload_size

    static std::vector<std::uint8_t> encode(
        const ReliableFrame& frame,
        std::uint32_t max_payload_bytes,
        std::string* error_message);

    static bool decode(
        const std::vector<std::uint8_t>& data,
        std::uint32_t max_payload_bytes,
        ReliableFrame* frame,
        std::string* error_message);
};

}  // namespace runtime::net

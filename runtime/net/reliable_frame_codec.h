#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace runtime::net {

struct ReliableFrame {
    std::uint8_t version{};
    std::uint32_t message_id{};
    std::uint16_t flags{};
    std::uint64_t request_id{};
    std::uint64_t session_id{};
    std::vector<std::uint8_t> payload;
};

class ReliableFrameCodec {
public:
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

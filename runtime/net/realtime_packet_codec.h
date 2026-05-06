#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace runtime::net {

struct RealtimePacket {
    std::uint8_t version{};
    std::uint16_t flags{};
    std::uint32_t message_id{};
    std::uint32_t sequence{};
    std::uint64_t realtime_session_id{};
    std::vector<std::uint8_t> payload;
};

class RealtimePacketCodec {
public:
    static std::vector<std::uint8_t> encode(
        const RealtimePacket& packet,
        std::uint32_t max_payload_bytes,
        std::string* error_message);

    static bool decode(
        const std::vector<std::uint8_t>& data,
        std::uint32_t max_payload_bytes,
        RealtimePacket* packet,
        std::string* error_message);
};

}  // namespace runtime::net

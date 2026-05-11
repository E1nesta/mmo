#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "runtime/protocol/message_mode.h"

namespace runtime::protocol {

inline constexpr std::uint8_t kProtocolVersion = 1;
inline constexpr std::uint32_t kErrorResponseMessageId = 9001;

struct FrameHeader {
    std::uint8_t version{kProtocolVersion};
    MessageMode mode{MessageMode::kCall};
    std::uint16_t flags{};
    std::uint32_t message_id{};
    std::uint64_t request_id{};
    std::uint64_t route_key{};
};

struct FrameMessage {
    FrameHeader header;
    std::string payload;
    std::vector<std::string> payload_batch;

    std::uint32_t message_id() const { return header.message_id; }
    std::uint64_t request_id() const { return header.request_id; }
    std::uint64_t route_key() const { return header.route_key; }
    MessageMode mode() const { return header.mode; }
};

inline bool is_cast_like(MessageMode mode) {
    return mode == MessageMode::kCast || mode == MessageMode::kBatch;
}

}  // namespace runtime::protocol

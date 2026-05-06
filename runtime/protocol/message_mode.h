#pragma once

#include <cstdint>

namespace runtime::protocol {

inline constexpr std::uint8_t kMessageModeCast = 0;
inline constexpr std::uint8_t kMessageModeCall = 1;
inline constexpr std::uint8_t kMessageModeReply = 2;
inline constexpr std::uint8_t kMessageModeBatch = 3;

enum class MessageMode : std::uint8_t {
    kCast = kMessageModeCast,
    kCall = kMessageModeCall,
    kReply = kMessageModeReply,
    kBatch = kMessageModeBatch,
};

inline std::uint8_t message_mode_value(MessageMode mode) {
    return static_cast<std::uint8_t>(mode);
}

}  // namespace runtime::protocol

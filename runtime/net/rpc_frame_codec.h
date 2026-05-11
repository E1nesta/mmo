#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "runtime/protocol/frame.h"

namespace runtime::net {

class RpcFrameCodec {
public:
    static constexpr std::uint32_t kFrameLengthBytes = 4;
    static constexpr std::uint32_t kRpcFrameOverheadBytes =
        sizeof(std::uint8_t) +   // version
        sizeof(std::uint8_t) +   // mode
        sizeof(std::uint16_t) +  // flags
        sizeof(std::uint32_t) +  // message_id
        sizeof(std::uint64_t) +  // request_id
        sizeof(std::uint64_t) +  // route_key
        sizeof(std::uint32_t);   // payload_size

    static std::vector<std::uint8_t> encode_rpc_frame(
        const runtime::protocol::FrameMessage& frame,
        std::uint32_t max_payload_bytes,
        std::string* error_message);

    static bool decode_rpc_frame(
        const std::vector<std::uint8_t>& data,
        std::uint32_t max_payload_bytes,
        runtime::protocol::FrameMessage* frame,
        std::string* error_message);
};

}  // namespace runtime::net

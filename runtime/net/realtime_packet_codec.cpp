#include "runtime/net/realtime_packet_codec.h"

#include <cstddef>
#include <utility>

namespace runtime::net {

namespace {

constexpr std::size_t kRealtimePacketHeaderBytes =
    sizeof(std::uint8_t) +   // version
    sizeof(std::uint16_t) +  // flags
    sizeof(std::uint16_t) +  // message_id
    sizeof(std::uint32_t) +  // sequence
    sizeof(std::uint64_t) +  // realtime_session_id
    sizeof(std::uint32_t);   // payload_size

void set_error(std::string* error_message, const std::string& message) {
    if (error_message != nullptr) {
        *error_message = message;
    }
}

void clear_error(std::string* error_message) {
    if (error_message != nullptr) {
        error_message->clear();
    }
}

void write_u8(std::vector<std::uint8_t>* out, std::uint8_t value) {
    out->push_back(value);
}

void write_u16(std::vector<std::uint8_t>* out, std::uint16_t value) {
    out->push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
    out->push_back(static_cast<std::uint8_t>(value & 0xffU));
}

void write_u32(std::vector<std::uint8_t>* out, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        out->push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void write_u64(std::vector<std::uint8_t>* out, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out->push_back(static_cast<std::uint8_t>((value >> shift) & 0xffULL));
    }
}

bool read_u8(
    const std::vector<std::uint8_t>& data,
    std::size_t* offset,
    std::uint8_t* value) {
    if (*offset + sizeof(std::uint8_t) > data.size()) {
        return false;
    }
    *value = data[*offset];
    *offset += sizeof(std::uint8_t);
    return true;
}

bool read_u16(
    const std::vector<std::uint8_t>& data,
    std::size_t* offset,
    std::uint16_t* value) {
    if (*offset + sizeof(std::uint16_t) > data.size()) {
        return false;
    }
    *value = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[*offset]) << 8U) |
        static_cast<std::uint16_t>(data[*offset + 1U]));
    *offset += sizeof(std::uint16_t);
    return true;
}

bool read_u32(
    const std::vector<std::uint8_t>& data,
    std::size_t* offset,
    std::uint32_t* value) {
    if (*offset + sizeof(std::uint32_t) > data.size()) {
        return false;
    }
    std::uint32_t result = 0;
    for (int index = 0; index < 4; ++index) {
        result = (result << 8U) | data[*offset + static_cast<std::size_t>(index)];
    }
    *value = result;
    *offset += sizeof(std::uint32_t);
    return true;
}

bool read_u64(
    const std::vector<std::uint8_t>& data,
    std::size_t* offset,
    std::uint64_t* value) {
    if (*offset + sizeof(std::uint64_t) > data.size()) {
        return false;
    }
    std::uint64_t result = 0;
    for (int index = 0; index < 8; ++index) {
        result = (result << 8U) | data[*offset + static_cast<std::size_t>(index)];
    }
    *value = result;
    *offset += sizeof(std::uint64_t);
    return true;
}

}  // namespace

std::vector<std::uint8_t> RealtimePacketCodec::encode(
    const RealtimePacket& packet,
    std::uint32_t max_payload_bytes,
    std::string* error_message) {
    clear_error(error_message);
    if (packet.payload.size() > max_payload_bytes) {
        set_error(error_message, "realtime packet payload exceeds limit");
        return {};
    }

    std::vector<std::uint8_t> out;
    out.reserve(kRealtimePacketHeaderBytes + packet.payload.size());
    write_u8(&out, packet.version);
    write_u16(&out, packet.flags);
    write_u16(&out, packet.message_id);
    write_u32(&out, packet.sequence);
    write_u64(&out, packet.realtime_session_id);
    write_u32(&out, static_cast<std::uint32_t>(packet.payload.size()));
    out.insert(out.end(), packet.payload.begin(), packet.payload.end());
    return out;
}

bool RealtimePacketCodec::decode(
    const std::vector<std::uint8_t>& data,
    std::uint32_t max_payload_bytes,
    RealtimePacket* packet,
    std::string* error_message) {
    clear_error(error_message);
    if (packet == nullptr) {
        set_error(error_message, "realtime packet output is null");
        return false;
    }
    if (data.size() < kRealtimePacketHeaderBytes) {
        set_error(error_message, "realtime packet is truncated");
        return false;
    }

    std::size_t offset = 0;
    std::uint32_t payload_size = 0;
    RealtimePacket decoded;
    if (!read_u8(data, &offset, &decoded.version) ||
        !read_u16(data, &offset, &decoded.flags) ||
        !read_u16(data, &offset, &decoded.message_id) ||
        !read_u32(data, &offset, &decoded.sequence) ||
        !read_u64(data, &offset, &decoded.realtime_session_id) ||
        !read_u32(data, &offset, &payload_size)) {
        set_error(error_message, "realtime packet header is truncated");
        return false;
    }

    if (payload_size > max_payload_bytes) {
        set_error(error_message, "realtime packet payload exceeds limit");
        return false;
    }
    if (payload_size != data.size() - offset) {
        set_error(error_message, "realtime packet payload size mismatch");
        return false;
    }

    decoded.payload.assign(data.begin() + static_cast<std::ptrdiff_t>(offset), data.end());
    *packet = std::move(decoded);
    return true;
}

}  // namespace runtime::net

#include "runtime/net/reliable_frame_codec.h"

#include <cstddef>
#include <utility>

namespace runtime::net {

namespace {

constexpr std::size_t kReliableFrameHeaderBytes =
    sizeof(std::uint32_t) +  // frame length after this field
    sizeof(std::uint8_t) +   // version
    sizeof(std::uint32_t) +  // message_id
    sizeof(std::uint16_t) +  // flags
    sizeof(std::uint64_t) +  // request_id
    sizeof(std::uint64_t) +  // session_id
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

std::vector<std::uint8_t> ReliableFrameCodec::encode(
    const ReliableFrame& frame,
    std::uint32_t max_payload_bytes,
    std::string* error_message) {
    clear_error(error_message);
    if (frame.payload.size() > max_payload_bytes) {
        set_error(error_message, "reliable frame payload exceeds limit");
        return {};
    }

    const auto frame_length =
        static_cast<std::uint32_t>(
            kReliableFrameHeaderBytes - sizeof(std::uint32_t) +
            frame.payload.size());
    std::vector<std::uint8_t> out;
    out.reserve(kReliableFrameHeaderBytes + frame.payload.size());
    write_u32(&out, frame_length);
    write_u8(&out, frame.version);
    write_u32(&out, frame.message_id);
    write_u16(&out, frame.flags);
    write_u64(&out, frame.request_id);
    write_u64(&out, frame.session_id);
    write_u32(&out, static_cast<std::uint32_t>(frame.payload.size()));
    out.insert(out.end(), frame.payload.begin(), frame.payload.end());
    return out;
}

bool ReliableFrameCodec::decode(
    const std::vector<std::uint8_t>& data,
    std::uint32_t max_payload_bytes,
    ReliableFrame* frame,
    std::string* error_message) {
    clear_error(error_message);
    if (frame == nullptr) {
        set_error(error_message, "reliable frame output is null");
        return false;
    }
    if (data.size() < kReliableFrameHeaderBytes) {
        set_error(error_message, "reliable frame is truncated");
        return false;
    }

    std::size_t offset = 0;
    std::uint32_t frame_length = 0;
    std::uint32_t payload_size = 0;
    ReliableFrame decoded;
    if (!read_u32(data, &offset, &frame_length) ||
        !read_u8(data, &offset, &decoded.version) ||
        !read_u32(data, &offset, &decoded.message_id) ||
        !read_u16(data, &offset, &decoded.flags) ||
        !read_u64(data, &offset, &decoded.request_id) ||
        !read_u64(data, &offset, &decoded.session_id) ||
        !read_u32(data, &offset, &payload_size)) {
        set_error(error_message, "reliable frame header is truncated");
        return false;
    }

    if (frame_length != data.size() - sizeof(std::uint32_t)) {
        set_error(error_message, "reliable frame length mismatch");
        return false;
    }
    if (payload_size > max_payload_bytes) {
        set_error(error_message, "reliable frame payload exceeds limit");
        return false;
    }
    if (offset + payload_size != data.size()) {
        set_error(error_message, "reliable frame payload size mismatch");
        return false;
    }

    decoded.payload.assign(data.begin() + static_cast<std::ptrdiff_t>(offset), data.end());
    *frame = std::move(decoded);
    return true;
}

}  // namespace runtime::net

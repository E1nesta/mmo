#include "runtime/net/frame_codec.h"

#include <cstddef>
#include <utility>

namespace runtime::net {

namespace {

constexpr std::size_t kRpcHeaderBytes =
    sizeof(std::uint32_t) +  // frame length after this field
    sizeof(std::uint8_t) +   // version
    sizeof(std::uint8_t) +   // mode
    sizeof(std::uint16_t) +  // flags
    sizeof(std::uint32_t) +  // message_id
    sizeof(std::uint64_t) +  // request_id
    sizeof(std::uint64_t) +  // route_key
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

bool encode_body(
    const runtime::protocol::FrameMessage& frame,
    std::vector<std::uint8_t>* body,
    std::uint32_t max_payload_bytes,
    std::string* error_message) {
    if (frame.header.mode != runtime::protocol::MessageMode::kBatch) {
        if (frame.payload.size() > max_payload_bytes) {
            set_error(error_message, "rpc frame payload exceeds limit");
            return false;
        }
        body->insert(body->end(), frame.payload.begin(), frame.payload.end());
        return true;
    }

    write_u32(body, static_cast<std::uint32_t>(frame.payload_batch.size()));
    for (const auto& payload : frame.payload_batch) {
        if (payload.size() > max_payload_bytes) {
            set_error(error_message, "rpc batch item exceeds limit");
            return false;
        }
        write_u32(body, static_cast<std::uint32_t>(payload.size()));
        body->insert(body->end(), payload.begin(), payload.end());
    }
    if (body->size() > max_payload_bytes) {
        set_error(error_message, "rpc batch body exceeds limit");
        return false;
    }
    return true;
}

bool decode_batch_body(
    const std::vector<std::uint8_t>& data,
    std::size_t offset,
    runtime::protocol::FrameMessage* frame,
    std::string* error_message) {
    std::uint32_t count = 0;
    if (!read_u32(data, &offset, &count)) {
        set_error(error_message, "rpc batch count is truncated");
        return false;
    }
    frame->payload_batch.clear();
    frame->payload_batch.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::uint32_t payload_size = 0;
        if (!read_u32(data, &offset, &payload_size) ||
            offset + payload_size > data.size()) {
            set_error(error_message, "rpc batch payload is truncated");
            return false;
        }
        frame->payload_batch.emplace_back(
            data.begin() + static_cast<std::ptrdiff_t>(offset),
            data.begin() + static_cast<std::ptrdiff_t>(offset + payload_size));
        offset += payload_size;
    }
    if (offset != data.size()) {
        set_error(error_message, "rpc batch body has trailing bytes");
        return false;
    }
    return true;
}

}  // namespace

std::vector<std::uint8_t> FrameCodec::encode_rpc_frame(
    const runtime::protocol::FrameMessage& frame,
    std::uint32_t max_payload_bytes,
    std::string* error_message) {
    clear_error(error_message);

    std::vector<std::uint8_t> body;
    if (!encode_body(frame, &body, max_payload_bytes, error_message)) {
        return {};
    }

    const auto frame_length = static_cast<std::uint32_t>(
        kRpcHeaderBytes - sizeof(std::uint32_t) + body.size());
    std::vector<std::uint8_t> out;
    out.reserve(kRpcHeaderBytes + body.size());
    write_u32(&out, frame_length);
    write_u8(&out, frame.header.version);
    write_u8(&out, runtime::protocol::message_mode_value(frame.header.mode));
    write_u16(&out, frame.header.flags);
    write_u32(&out, frame.header.message_id);
    write_u64(&out, frame.header.request_id);
    write_u64(&out, frame.header.route_key);
    write_u32(&out, static_cast<std::uint32_t>(body.size()));
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

bool FrameCodec::decode_rpc_frame(
    const std::vector<std::uint8_t>& data,
    std::uint32_t max_payload_bytes,
    runtime::protocol::FrameMessage* frame,
    std::string* error_message) {
    clear_error(error_message);
    if (frame == nullptr) {
        set_error(error_message, "rpc frame output is null");
        return false;
    }
    if (data.size() < kRpcHeaderBytes) {
        set_error(error_message, "rpc frame is truncated");
        return false;
    }

    std::size_t offset = 0;
    std::uint32_t frame_length = 0;
    std::uint8_t mode = 0;
    std::uint32_t payload_size = 0;
    runtime::protocol::FrameMessage decoded;
    if (!read_u32(data, &offset, &frame_length) ||
        !read_u8(data, &offset, &decoded.header.version) ||
        !read_u8(data, &offset, &mode) ||
        !read_u16(data, &offset, &decoded.header.flags) ||
        !read_u32(data, &offset, &decoded.header.message_id) ||
        !read_u64(data, &offset, &decoded.header.request_id) ||
        !read_u64(data, &offset, &decoded.header.route_key) ||
        !read_u32(data, &offset, &payload_size)) {
        set_error(error_message, "rpc frame header is truncated");
        return false;
    }

    if (frame_length != data.size() - sizeof(std::uint32_t)) {
        set_error(error_message, "rpc frame length mismatch");
        return false;
    }
    if (payload_size > max_payload_bytes) {
        set_error(error_message, "rpc frame payload exceeds limit");
        return false;
    }
    if (offset + payload_size != data.size()) {
        set_error(error_message, "rpc frame payload size mismatch");
        return false;
    }

    decoded.header.mode = static_cast<runtime::protocol::MessageMode>(mode);
    if (decoded.header.mode == runtime::protocol::MessageMode::kBatch) {
        if (!decode_batch_body(data, offset, &decoded, error_message)) {
            return false;
        }
    } else {
        decoded.payload.assign(
            data.begin() + static_cast<std::ptrdiff_t>(offset),
            data.end());
    }
    *frame = std::move(decoded);
    return true;
}

}  // namespace runtime::net

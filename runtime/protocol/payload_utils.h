#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/message.h>

#include "common/error.pb.h"
#include "runtime/protocol/frame.h"

namespace runtime::protocol {

std::int64_t current_time_millis();

mmo::common::Result make_ok_result();

mmo::common::Result make_error_result(
    int error_code,
    const std::string& error_message);

FrameMessage make_error_frame(
    const FrameMessage& request,
    int error_code,
    const std::string& error_message);

template <typename Message>
bool serialize_payload(
    const Message& message,
    std::string* payload,
    std::string* error_message = nullptr) {
    if (payload == nullptr) {
        if (error_message != nullptr) {
            *error_message = "payload output is null";
        }
        return false;
    }
    payload->clear();
    if (!message.SerializeToString(payload)) {
        if (error_message != nullptr) {
            *error_message = "failed to serialize protobuf payload";
        }
        return false;
    }
    return true;
}

template <typename Message>
bool parse_payload(
    const FrameMessage& frame,
    Message* message,
    std::string* error_message = nullptr) {
    if (message == nullptr) {
        if (error_message != nullptr) {
            *error_message = "protobuf output is null";
        }
        return false;
    }
    if (!message->ParseFromString(frame.payload)) {
        if (error_message != nullptr) {
            *error_message = "failed to parse protobuf payload";
        }
        return false;
    }
    return true;
}

template <typename Message>
FrameMessage pack_message(
    std::uint32_t message_id,
    std::uint64_t request_id,
    std::uint64_t route_key,
    MessageMode mode,
    const Message& message) {
    FrameMessage frame;
    frame.header.message_id = message_id;
    frame.header.request_id = request_id;
    frame.header.route_key = route_key;
    frame.header.mode = mode;
    message.SerializeToString(&frame.payload);
    return frame;
}

template <typename Message>
FrameMessage pack_message(
    std::uint32_t message_id,
    std::uint64_t request_id,
    std::uint64_t route_key,
    const Message& message) {
    return pack_message(
        message_id,
        request_id,
        route_key,
        MessageMode::kCall,
        message);
}

template <typename Message>
FrameMessage pack_batch(
    std::uint32_t message_id,
    std::uint64_t request_id,
    std::uint64_t route_key,
    const std::vector<Message>& messages) {
    FrameMessage frame;
    frame.header.message_id = message_id;
    frame.header.request_id = request_id;
    frame.header.route_key = route_key;
    frame.header.mode = MessageMode::kBatch;
    frame.payload_batch.reserve(messages.size());
    for (const auto& message : messages) {
        std::string payload;
        message.SerializeToString(&payload);
        frame.payload_batch.push_back(std::move(payload));
    }
    return frame;
}

}  // namespace runtime::protocol

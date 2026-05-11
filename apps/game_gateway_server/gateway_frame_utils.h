#pragma once

#include <cstdint>
#include <string>

#include <google/protobuf/message.h>

#include "runtime/net/reliable_frame_codec.h"

namespace apps::game_gateway_server {

template <typename Message>
bool parse_gateway_payload(
    const runtime::net::ReliableFrame& frame,
    Message* message) {
    if (message == nullptr) {
        return false;
    }
    return message->ParseFromArray(
        frame.payload.data(),
        static_cast<int>(frame.payload.size()));
}

inline runtime::net::ReliableFrame make_gateway_payload_frame(
    const runtime::net::ReliableFrame& request,
    std::uint16_t message_id,
    const google::protobuf::Message& message) {
    runtime::net::ReliableFrame response;
    response.version = request.version;
    response.flags = request.flags;
    response.message_id = message_id;
    response.request_id = request.request_id;
    response.session_id = request.session_id;

    std::string payload;
    message.SerializeToString(&payload);
    response.payload.assign(payload.begin(), payload.end());
    return response;
}

}  // namespace apps::game_gateway_server

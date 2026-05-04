#pragma once

#include <string>

#include <google/protobuf/message.h>

#include "common/context.pb.h"
#include "common/envelope.pb.h"

namespace runtime::protocol {

inline constexpr const char* kErrorResponse = "common.ResponseContext";

mmo::common::ResponseContext make_ok_context(
    const mmo::common::RequestContext& request);

mmo::common::ResponseContext make_error_context(
    const mmo::common::RequestContext& request,
    int error_code,
    const std::string& error_message);

mmo::common::Envelope make_error_envelope(
    const mmo::common::Envelope& request,
    int error_code,
    const std::string& error_message);

template <typename Message>
mmo::common::Envelope pack_message(
    const std::string& message_type,
    const mmo::common::RequestContext& context,
    const Message& message) {
    mmo::common::Envelope envelope;
    envelope.set_request_id(context.request_id());
    envelope.set_message_type(message_type);
    envelope.set_player_id(context.player_id());
    envelope.set_session_token(context.session_token());
    envelope.set_game_session_id(context.game_session_id());
    envelope.set_trace_id(context.trace_id());
    message.SerializeToString(envelope.mutable_payload());
    return envelope;
}

template <typename Message>
bool unpack_message(const mmo::common::Envelope& envelope, Message& message) {
    return message.ParseFromString(envelope.payload());
}

}  // namespace runtime::protocol

#pragma once

#include <string>

#include <google/protobuf/message.h>

#include "public/common.pb.h"

namespace mmo::runtime::protocol {

mmo::public_api::ResponseContext make_ok_context(
    const mmo::public_api::RequestContext& request);

mmo::public_api::ResponseContext make_error_context(
    const mmo::public_api::RequestContext& request,
    int error_code,
    const std::string& error_message);

mmo::public_api::Envelope make_error_envelope(
    const mmo::public_api::Envelope& request,
    int error_code,
    const std::string& error_message);

template <typename Message>
mmo::public_api::Envelope pack_message(
    const std::string& message_type,
    const mmo::public_api::RequestContext& context,
    const Message& message) {
    mmo::public_api::Envelope envelope;
    envelope.set_request_id(context.request_id());
    envelope.set_message_type(message_type);
    envelope.set_player_id(context.player_id());
    envelope.set_session_token(context.session_token());
    message.SerializeToString(envelope.mutable_payload());
    return envelope;
}

template <typename Message>
bool unpack_message(const mmo::public_api::Envelope& envelope, Message& message) {
    return message.ParseFromString(envelope.payload());
}

}  // namespace mmo::runtime::protocol

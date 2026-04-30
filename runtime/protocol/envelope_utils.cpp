#include "runtime/protocol/envelope_utils.h"

#include "runtime/protocol/message_types.h"

namespace mmo::runtime::protocol {

mmo::common::ResponseContext make_ok_context(
    const mmo::common::RequestContext& request) {
    mmo::common::ResponseContext response;
    response.set_request_id(request.request_id());
    response.set_account_id(request.account_id());
    response.set_player_id(request.player_id());
    response.set_session_token(request.session_token());
    response.set_trace_id(request.trace_id());
    response.set_game_session_id(request.game_session_id());
    response.set_success(true);
    return response;
}

mmo::common::ResponseContext make_error_context(
    const mmo::common::RequestContext& request,
    int error_code,
    const std::string& error_message) {
    auto response = make_ok_context(request);
    response.set_success(false);
    response.set_error_code(error_code);
    response.set_error_message(error_message);
    return response;
}

mmo::common::Envelope make_error_envelope(
    const mmo::common::Envelope& request,
    int error_code,
    const std::string& error_message) {
    mmo::common::RequestContext context;
    context.set_request_id(request.request_id());
    context.set_player_id(request.player_id());
    context.set_session_token(request.session_token());
    context.set_game_session_id(request.game_session_id());
    context.set_trace_id(request.trace_id());

    mmo::common::ResponseContext response =
        make_error_context(context, error_code, error_message);

    mmo::common::Envelope envelope;
    envelope.set_request_id(request.request_id());
    envelope.set_message_type(kErrorResponse);
    envelope.set_player_id(request.player_id());
    envelope.set_session_token(request.session_token());
    envelope.set_game_session_id(request.game_session_id());
    envelope.set_trace_id(request.trace_id());
    response.SerializeToString(envelope.mutable_payload());
    return envelope;
}

}  // namespace mmo::runtime::protocol

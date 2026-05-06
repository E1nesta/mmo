#include "runtime/protocol/payload_utils.h"

#include <chrono>

namespace runtime::protocol {

std::int64_t current_time_millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

mmo::common::Result make_ok_result() {
    mmo::common::Result result;
    result.set_ok(true);
    return result;
}

mmo::common::Result make_error_result(
    int error_code,
    const std::string& error_message) {
    mmo::common::Result result;
    result.set_ok(false);
    auto* error = result.mutable_error();
    error->set_code(error_code);
    error->set_message(error_message);
    return result;
}

FrameMessage make_error_frame(
    const FrameMessage& request,
    int error_code,
    const std::string& error_message) {
    FrameMessage frame;
    frame.header.message_id = kErrorResponseMessageId;
    frame.header.request_id = request.header.request_id;
    frame.header.route_key = request.header.route_key;
    frame.header.mode = MessageMode::kReply;
    const auto result = make_error_result(error_code, error_message);
    result.SerializeToString(&frame.payload);
    return frame;
}

}  // namespace runtime::protocol

#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "runtime/protocol/frame.h"

namespace runtime::handler {

struct HandlerContext {
    std::string service_name;
    std::uint32_t message_id{};
    std::uint64_t request_id{};
    std::uint64_t route_key{};
    runtime::protocol::FrameMessage frame;
};

inline HandlerContext make_handler_context(
    std::string service_name,
    const runtime::protocol::FrameMessage& frame) {
    return HandlerContext{
        std::move(service_name),
        frame.message_id(),
        frame.request_id(),
        frame.route_key(),
        frame};
}

}  // namespace runtime::handler

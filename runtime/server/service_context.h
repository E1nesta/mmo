#pragma once

#include <string>
#include <utility>

#include "common/context.pb.h"
#include "common/envelope.pb.h"

namespace runtime::server {

struct ServiceContext {
    std::string service_name;
    std::string message_type;
    mmo::common::RequestContext request;
};

inline ServiceContext make_service_context(
    std::string service_name,
    const mmo::common::Envelope& envelope,
    const mmo::common::RequestContext& request) {
    return ServiceContext{
        std::move(service_name),
        envelope.message_type(),
        request};
}

}  // namespace runtime::server

#pragma once

#include <string>

#include "common/context.pb.h"

namespace mmo::runtime::rpc {

struct RpcContext {
    mmo::common::RequestContext request;
    std::string source_service;
    std::string target_service;
    std::string trace_id;
};

}  // namespace mmo::runtime::rpc

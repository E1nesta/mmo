#pragma once

#include <cstdint>
#include <string>

namespace runtime::rpc {

struct RpcContext {
    std::string source_service;
    std::string target_service;
    std::uint64_t request_id{};
    std::uint64_t route_key{};
};

}  // namespace runtime::rpc

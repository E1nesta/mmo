#include "runtime/gateway/gateway_context.h"

namespace runtime::gateway {

runtime::rpc::RpcOptions make_gateway_rpc_options(
    const GatewayRoute& route,
    std::uint64_t route_key) {
    runtime::rpc::RpcOptions options;
    options.routing_policy = route.routing_policy;
    options.target_instance_id = route.target_instance_id;
    options.mode = route.mode;
    options.route_key = route_key;
    return options;
}

}  // namespace runtime::gateway

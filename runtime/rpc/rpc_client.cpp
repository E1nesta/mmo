#include "runtime/rpc/rpc_client.h"

#include <utility>

namespace runtime::rpc {

RpcClientOptions make_rpc_client_options(
    const std::string& source_service,
    const runtime::foundation::ServerConfig& config) {
    (void)config;
    RpcClientOptions options;
    options.source_service = source_service;
    return options;
}

RpcClient::RpcClient(
    std::shared_ptr<runtime::rpc::RpcServiceRegistry> service_registry,
    runtime::net::TransportOptions transport_options,
    runtime::rpc::RpcConnectionPoolOptions pool_options,
    RpcClientOptions options)
    : connection_pool_(
          std::move(service_registry),
          transport_options,
          pool_options),
      options_(std::move(options)) {}

RpcResult RpcClient::call_frame(
    const std::string& target_service,
    runtime::protocol::FrameMessage request,
    RpcOptions options) {
    request.header.mode = runtime::protocol::MessageMode::kCall;
    request.header.request_id = request.header.request_id != 0
        ? request.header.request_id
        : request_id_or_next(options);
    request.header.route_key = request.header.route_key != 0
        ? request.header.route_key
        : options.route_key;
    return connection_pool_.call(
        target_service, request, make_call_options(options));
}

RpcResult RpcClient::cast_frame(
    const std::string& target_service,
    runtime::protocol::FrameMessage request,
    RpcOptions options) {
    request.header.request_id = request.header.request_id != 0
        ? request.header.request_id
        : request_id_or_next(options);
    request.header.route_key = request.header.route_key != 0
        ? request.header.route_key
        : options.route_key;
    return connection_pool_.cast(
        target_service, request, make_call_options(options));
}

std::uint64_t RpcClient::request_id_or_next(const RpcOptions& options) {
    if (options.request_id != 0) {
        return options.request_id;
    }
    return next_request_id_.fetch_add(1, std::memory_order_relaxed);
}

RpcCallOptions RpcClient::make_call_options(const RpcOptions& options) const {
    RpcCallOptions call_options;
    call_options.source_service = options.source_service.empty()
        ? options_.source_service
        : options.source_service;
    call_options.target_service = options.target_service;
    call_options.routing_policy = options.routing_policy;
    call_options.route_key = options.route_key;
    call_options.target_instance_id = options.target_instance_id;
    call_options.connect_timeout_millis = options.connect_timeout_millis;
    call_options.request_timeout_millis = options.request_timeout_millis;
    call_options.retry_enabled = options.retry_enabled;
    return call_options;
}

}  // namespace runtime::rpc

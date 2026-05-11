#include "runtime/rpc/rpc_client.h"

#include <utility>

namespace runtime::rpc {

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
        target_service, request, make_effective_options(std::move(options)));
}

RpcError RpcClient::call_frame_async(
    const std::string& target_service,
    runtime::protocol::FrameMessage request,
    RpcOptions options,
    RpcResultHandler handler) {
    request.header.mode = runtime::protocol::MessageMode::kCall;
    request.header.request_id = request.header.request_id != 0
        ? request.header.request_id
        : request_id_or_next(options);
    request.header.route_key = request.header.route_key != 0
        ? request.header.route_key
        : options.route_key;
    return connection_pool_.call_async(
        target_service,
        request,
        make_effective_options(std::move(options)),
        std::move(handler));
}

RpcResult RpcClient::cast_frame(
    const std::string& target_service,
    runtime::protocol::FrameMessage request,
    RpcOptions options) {
    if (!runtime::protocol::is_cast_like(request.header.mode)) {
        request.header.mode = runtime::protocol::MessageMode::kCast;
    }
    request.header.request_id = request.header.request_id != 0
        ? request.header.request_id
        : request_id_or_next(options);
    request.header.route_key = request.header.route_key != 0
        ? request.header.route_key
        : options.route_key;
    return connection_pool_.cast(
        target_service, request, make_effective_options(std::move(options)));
}

std::uint64_t RpcClient::request_id_or_next(const RpcOptions& options) {
    if (options.request_id != 0) {
        return options.request_id;
    }
    return next_request_id_.fetch_add(1, std::memory_order_relaxed);
}

RpcOptions RpcClient::make_effective_options(RpcOptions options) const {
    if (options.source_service.empty()) {
        options.source_service = options_.source_service;
    }
    return options;
}

}  // namespace runtime::rpc

#include "runtime/rpc/rpc_client.h"

#include <utility>

#include "runtime/protocol/internal_auth.h"

namespace mmo::runtime::rpc {
namespace {

RpcErrorCode to_rpc_error_code(
    mmo::runtime::channel::ChannelErrorCode code) {
    switch (code) {
        case mmo::runtime::channel::ChannelErrorCode::kOk:
            return RpcErrorCode::kOk;
        case mmo::runtime::channel::ChannelErrorCode::kEndpointNotFound:
            return RpcErrorCode::kEndpointNotFound;
        case mmo::runtime::channel::ChannelErrorCode::kConnectFailed:
            return RpcErrorCode::kConnectFailed;
        case mmo::runtime::channel::ChannelErrorCode::kWriteFailed:
            return RpcErrorCode::kWriteFailed;
        case mmo::runtime::channel::ChannelErrorCode::kReadFailed:
            return RpcErrorCode::kReadFailed;
        case mmo::runtime::channel::ChannelErrorCode::kEncodeFailed:
            return RpcErrorCode::kEncodeFailed;
        case mmo::runtime::channel::ChannelErrorCode::kDecodeFailed:
            return RpcErrorCode::kDecodeFailed;
        case mmo::runtime::channel::ChannelErrorCode::kTimeout:
            return RpcErrorCode::kTimeout;
        case mmo::runtime::channel::ChannelErrorCode::kRemoteError:
            return RpcErrorCode::kRemoteError;
        case mmo::runtime::channel::ChannelErrorCode::kRequestIdMismatch:
            return RpcErrorCode::kRequestIdMismatch;
        case mmo::runtime::channel::ChannelErrorCode::kChannelClosed:
            return RpcErrorCode::kChannelClosed;
        case mmo::runtime::channel::ChannelErrorCode::kPendingLimitExceeded:
            return RpcErrorCode::kPendingLimitExceeded;
        case mmo::runtime::channel::ChannelErrorCode::kDuplicateRequestId:
            return RpcErrorCode::kDuplicateRequestId;
    }
    return RpcErrorCode::kChannelClosed;
}

RpcError to_rpc_error(const mmo::runtime::channel::ChannelError& error) {
    return make_rpc_error(to_rpc_error_code(error.code), error.message);
}

}  // namespace

RpcClientOptions make_rpc_client_options(
    const std::string& source_service,
    const mmo::runtime::foundation::ServerConfig& config) {
    RpcClientOptions options;
    options.source_service = source_service;
    options.internal_auth_shared_secret =
        config.security.internal_auth.shared_secret;
    return options;
}

RpcClient::RpcClient(
    std::shared_ptr<mmo::runtime::channel::EndpointResolver> resolver,
    mmo::runtime::transport::TransportOptions transport_options,
    mmo::runtime::channel::ChannelConnectionPoolOptions pool_options,
    RpcClientOptions options)
    : channel_client_(std::make_unique<mmo::runtime::channel::TcpChannelClient>(
          std::move(resolver),
          transport_options,
          pool_options)),
      options_(std::move(options)) {}

RpcClient::RpcClient(
    std::shared_ptr<mmo::runtime::channel::ServiceRegistry> service_registry,
    mmo::runtime::transport::TransportOptions transport_options,
    mmo::runtime::channel::ChannelConnectionPoolOptions pool_options,
    RpcClientOptions options)
    : channel_client_(std::make_unique<mmo::runtime::channel::TcpChannelClient>(
          std::move(service_registry),
          transport_options,
          pool_options)),
      options_(std::move(options)) {}

RpcClient::RpcClient(
    std::unique_ptr<mmo::runtime::channel::ChannelClient> channel_client,
    RpcClientOptions options)
    : channel_client_(std::move(channel_client)),
      options_(std::move(options)) {}

RpcResult RpcClient::call_envelope(
    const std::string& target_service,
    const mmo::common::Envelope& request,
    RpcController controller) {
    controller.target_service = target_service;

    mmo::runtime::channel::ChannelCallOptions options;
    options.source_service = controller.source_service;
    options.target_service = controller.target_service;
    options.trace_id = controller.trace_id;
    options.routing_policy = controller.routing_policy;
    options.route_key = controller.route_key;
    options.target_instance_id = controller.target_instance_id;
    options.connect_timeout_millis = controller.connect_timeout_millis;
    options.request_timeout_millis = controller.request_timeout_millis;
    options.retry_enabled = controller.retry_enabled;

    auto signed_request = request;
    const std::string source_service = !controller.source_service.empty()
                                           ? controller.source_service
                                           : options_.source_service;
    if (!options_.internal_auth_shared_secret.empty()) {
        std::string error_message;
        if (!mmo::runtime::protocol::sign_internal_envelope_now(
                &signed_request,
                source_service,
                options_.internal_auth_shared_secret,
                &error_message)) {
            return RpcResult::failure(make_rpc_error(
                RpcErrorCode::kEncodeFailed,
                "failed to sign internal rpc request: " + error_message));
        }
    }

    const auto channel_result =
        channel_client_->call_envelope(target_service, signed_request, options);
    if (channel_result.ok()) {
        return RpcResult::success(channel_result.response());
    }
    if (channel_result.has_response()) {
        return RpcResult::remote_error(channel_result.response());
    }
    return RpcResult::failure(to_rpc_error(channel_result.error()));
}

}  // namespace mmo::runtime::rpc

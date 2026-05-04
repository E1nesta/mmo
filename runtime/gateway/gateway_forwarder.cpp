#include "runtime/gateway/gateway_forwarder.h"

#include <utility>

#include "runtime/channel/channel_error.h"
#include "runtime/protocol/internal_auth.h"

namespace runtime::gateway {

GatewayForwarder::GatewayForwarder(
    std::string source_service,
    runtime::channel::ChannelClient& channel_client)
    : source_service_(std::move(source_service)),
      channel_client_(&channel_client) {}

GatewayForwarder::GatewayForwarder(
    std::string source_service,
    std::shared_ptr<runtime::channel::EndpointResolver> resolver,
    runtime::transport::TransportOptions transport_options,
    runtime::channel::ChannelConnectionPoolOptions pool_options)
    : source_service_(std::move(source_service)),
      owned_channel_client_(std::make_unique<runtime::channel::TcpChannelClient>(
          std::move(resolver),
          transport_options,
          pool_options)),
      channel_client_(owned_channel_client_.get()) {}

GatewayForwarder::GatewayForwarder(
    std::string source_service,
    std::shared_ptr<runtime::channel::ServiceRegistry> service_registry,
    runtime::transport::TransportOptions transport_options,
    runtime::channel::ChannelConnectionPoolOptions pool_options)
    : source_service_(std::move(source_service)),
      owned_channel_client_(std::make_unique<runtime::channel::TcpChannelClient>(
          std::move(service_registry),
          transport_options,
          pool_options)),
      channel_client_(owned_channel_client_.get()) {}

GatewayForwarder::GatewayForwarder(
    std::string source_service,
    const runtime::foundation::ServerConfig& config,
    runtime::transport::TransportOptions transport_options)
    : GatewayForwarder(
          std::move(source_service),
          std::make_shared<runtime::channel::StaticServiceRegistry>(config),
          transport_options,
          runtime::channel::make_channel_connection_pool_options(
              config.channel)) {
    internal_auth_shared_secret_ = config.security.internal_auth.shared_secret;
}

ProxyResult GatewayForwarder::forward_envelope(
    const std::string& target_service,
    const mmo::common::Envelope& envelope,
    runtime::channel::ChannelCallOptions options) {
    options.source_service = source_service_;
    options.target_service = target_service;

    auto signed_envelope = envelope;
    if (!internal_auth_shared_secret_.empty()) {
        std::string error_message;
        if (!runtime::protocol::sign_internal_envelope_now(
                &signed_envelope,
                source_service_,
                internal_auth_shared_secret_,
                &error_message)) {
            return ProxyResult::failure(runtime::channel::make_channel_error(
                runtime::channel::ChannelErrorCode::kEncodeFailed,
                "failed to sign internal gateway request: " + error_message));
        }
    }

    const auto channel_result =
        channel_client_->call_envelope(target_service, signed_envelope, options);
    if (!channel_result.ok()) {
        if (channel_result.has_response()) {
            return ProxyResult::remote_error(channel_result.response());
        }
        return ProxyResult::failure(channel_result.error());
    }
    return ProxyResult::success(channel_result.response());
}

}  // namespace runtime::gateway

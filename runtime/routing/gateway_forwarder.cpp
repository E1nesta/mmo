#include "runtime/routing/gateway_forwarder.h"

#include <utility>

#include "runtime/channel/channel_error.h"
#include "runtime/protocol/internal_auth.h"

namespace mmo::runtime::routing {

GatewayForwarder::GatewayForwarder(
    std::string source_service,
    mmo::runtime::channel::ChannelClient& channel_client)
    : source_service_(std::move(source_service)),
      channel_client_(&channel_client) {}

GatewayForwarder::GatewayForwarder(
    std::string source_service,
    std::shared_ptr<mmo::runtime::channel::EndpointResolver> resolver,
    mmo::runtime::transport::TransportOptions transport_options,
    mmo::runtime::channel::ChannelConnectionPoolOptions pool_options)
    : source_service_(std::move(source_service)),
      owned_channel_client_(std::make_unique<mmo::runtime::channel::TcpChannelClient>(
          std::move(resolver),
          transport_options,
          pool_options)),
      channel_client_(owned_channel_client_.get()) {}

GatewayForwarder::GatewayForwarder(
    std::string source_service,
    const mmo::runtime::foundation::ServerConfig& config,
    mmo::runtime::transport::TransportOptions transport_options)
    : GatewayForwarder(
          std::move(source_service),
          std::make_shared<mmo::runtime::channel::StaticEndpointResolver>(config),
          transport_options,
          mmo::runtime::channel::make_channel_connection_pool_options(
              config.channel)) {
    internal_auth_shared_secret_ = config.security.internal_auth.shared_secret;
}

ForwardResult GatewayForwarder::forward_envelope(
    const std::string& target_service,
    const mmo::common::Envelope& envelope) {
    mmo::runtime::channel::ChannelCallOptions options;
    options.source_service = source_service_;
    options.target_service = target_service;

    auto signed_envelope = envelope;
    if (!internal_auth_shared_secret_.empty()) {
        std::string error_message;
        if (!mmo::runtime::protocol::sign_internal_envelope_now(
                &signed_envelope,
                source_service_,
                internal_auth_shared_secret_,
                &error_message)) {
            return ForwardResult::failure(mmo::runtime::channel::make_channel_error(
                mmo::runtime::channel::ChannelErrorCode::kEncodeFailed,
                "failed to sign internal gateway request: " + error_message));
        }
    }

    const auto channel_result =
        channel_client_->call_envelope(target_service, signed_envelope, options);
    if (!channel_result.ok()) {
        if (channel_result.has_response()) {
            return ForwardResult::remote_error(channel_result.response());
        }
        return ForwardResult::failure(channel_result.error());
    }
    return ForwardResult::success(channel_result.response());
}

}  // namespace mmo::runtime::routing

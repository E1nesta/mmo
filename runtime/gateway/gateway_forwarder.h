#pragma once

#include <memory>
#include <string>
#include <utility>

#include "common/context.pb.h"
#include "common/envelope.pb.h"
#include "runtime/channel/channel_client.h"
#include "runtime/channel/endpoint_resolver.h"
#include "runtime/channel/service_registry.h"
#include "runtime/channel/static_endpoint_resolver.h"
#include "runtime/foundation/server_config.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/gateway/proxy_result.h"
#include "runtime/transport/envelope_transport.h"

namespace runtime::gateway {

class GatewayForwarder {
public:
    GatewayForwarder(
        std::string source_service,
        runtime::channel::ChannelClient& channel_client);
    GatewayForwarder(
        std::string source_service,
        std::shared_ptr<runtime::channel::EndpointResolver> resolver,
        runtime::transport::TransportOptions transport_options,
        runtime::channel::ChannelConnectionPoolOptions pool_options);
    GatewayForwarder(
        std::string source_service,
        std::shared_ptr<runtime::channel::ServiceRegistry> service_registry,
        runtime::transport::TransportOptions transport_options,
        runtime::channel::ChannelConnectionPoolOptions pool_options);
    GatewayForwarder(
        std::string source_service,
        const runtime::foundation::ServerConfig& config,
        runtime::transport::TransportOptions transport_options);
    GatewayForwarder(const GatewayForwarder&) = delete;
    GatewayForwarder& operator=(const GatewayForwarder&) = delete;
    GatewayForwarder(GatewayForwarder&&) = delete;
    GatewayForwarder& operator=(GatewayForwarder&&) = delete;

    template <typename Request>
    ProxyResult forward(
        const std::string& target_service,
        const std::string& target_message_type,
        const mmo::common::RequestContext& context,
        const Request& request) {
        return forward(
            target_service,
            target_message_type,
            context,
            request,
            runtime::channel::ChannelCallOptions{});
    }

    template <typename Request>
    ProxyResult forward(
        const std::string& target_service,
        const std::string& target_message_type,
        const mmo::common::RequestContext& context,
        const Request& request,
        runtime::channel::ChannelCallOptions options) {
        auto envelope = runtime::protocol::pack_message(
            target_message_type, context, request);
        return forward_envelope(target_service, envelope, std::move(options));
    }

    ProxyResult forward_envelope(
        const std::string& target_service,
        const mmo::common::Envelope& envelope,
        runtime::channel::ChannelCallOptions options = {});

private:
    std::string source_service_;
    std::string internal_auth_shared_secret_;
    std::unique_ptr<runtime::channel::ChannelClient> owned_channel_client_;
    runtime::channel::ChannelClient* channel_client_{};
};

}  // namespace runtime::gateway

#pragma once

#include <memory>
#include <string>

#include "common/context.pb.h"
#include "common/envelope.pb.h"
#include "runtime/channel/channel_client.h"
#include "runtime/channel/endpoint_resolver.h"
#include "runtime/channel/static_endpoint_resolver.h"
#include "runtime/foundation/server_config.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/routing/forward_result.h"
#include "runtime/transport/envelope_transport.h"

namespace mmo::runtime::routing {

class GatewayForwarder {
public:
    GatewayForwarder(
        std::string source_service,
        mmo::runtime::channel::ChannelClient& channel_client);
    GatewayForwarder(
        std::string source_service,
        std::shared_ptr<mmo::runtime::channel::EndpointResolver> resolver,
        mmo::runtime::transport::TransportOptions transport_options,
        mmo::runtime::channel::ChannelConnectionPoolOptions pool_options);
    GatewayForwarder(
        std::string source_service,
        const mmo::runtime::foundation::ServerConfig& config,
        mmo::runtime::transport::TransportOptions transport_options);
    GatewayForwarder(const GatewayForwarder&) = delete;
    GatewayForwarder& operator=(const GatewayForwarder&) = delete;
    GatewayForwarder(GatewayForwarder&&) = delete;
    GatewayForwarder& operator=(GatewayForwarder&&) = delete;

    template <typename Request>
    ForwardResult forward(
        const std::string& target_service,
        const std::string& target_message_type,
        const mmo::common::RequestContext& context,
        const Request& request) {
        auto envelope = mmo::runtime::protocol::pack_message(
            target_message_type, context, request);
        return forward_envelope(target_service, envelope);
    }

    ForwardResult forward_envelope(
        const std::string& target_service,
        const mmo::common::Envelope& envelope);

private:
    std::string source_service_;
    std::string internal_auth_shared_secret_;
    std::unique_ptr<mmo::runtime::channel::ChannelClient> owned_channel_client_;
    mmo::runtime::channel::ChannelClient* channel_client_{};
};

}  // namespace mmo::runtime::routing

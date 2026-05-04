#include "runtime/channel/channel_client.h"

#include <utility>

namespace runtime::channel {

TcpChannelClient::TcpChannelClient(
    std::shared_ptr<EndpointResolver> resolver,
    runtime::transport::TransportOptions transport_options,
    ChannelConnectionPoolOptions pool_options)
    : connection_pool_(
          std::move(resolver),
          transport_options,
          pool_options) {}

TcpChannelClient::TcpChannelClient(
    std::shared_ptr<ServiceRegistry> service_registry,
    runtime::transport::TransportOptions transport_options,
    ChannelConnectionPoolOptions pool_options)
    : connection_pool_(
          std::move(service_registry),
          transport_options,
          pool_options) {}

ChannelResult TcpChannelClient::call_envelope(
    const std::string& target_service,
    const mmo::common::Envelope& request,
    ChannelCallOptions options) {
    options.target_service = target_service;
    return connection_pool_.call(target_service, request, options);
}

std::size_t TcpChannelClient::pending_count(
    const std::string& target_service) const {
    return connection_pool_.pending_count(target_service);
}

std::size_t TcpChannelClient::pending_count_total() const {
    return connection_pool_.pending_count_total();
}

}  // namespace runtime::channel

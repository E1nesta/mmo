#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/channel/channel.h"
#include "runtime/channel/endpoint_resolver.h"
#include "runtime/channel/routing_policy.h"
#include "runtime/channel/service_registry.h"
#include "runtime/foundation/server_config.h"

namespace runtime::channel {

struct ChannelConnectionPoolOptions {
    int connect_timeout_millis{};
    int request_timeout_millis{};
    std::size_t connections_per_upstream{1};
    std::size_t max_pending_requests_per_connection{128};
    std::size_t max_pending_requests_per_upstream{256};
};

ChannelConnectionPoolOptions make_channel_connection_pool_options(
    const runtime::foundation::ChannelConfig& config);

class ChannelConnectionPool {
public:
    ChannelConnectionPool(
        std::shared_ptr<EndpointResolver> resolver,
        runtime::transport::TransportOptions transport_options,
        ChannelConnectionPoolOptions options);
    ChannelConnectionPool(
        std::shared_ptr<ServiceRegistry> service_registry,
        runtime::transport::TransportOptions transport_options,
        ChannelConnectionPoolOptions options);

    ChannelResult call(
        const std::string& target_service,
        const mmo::common::Envelope& request,
        const ChannelCallOptions& options);

    std::size_t pending_count(const std::string& target_service) const;
    std::size_t pending_count(
        const std::string& target_service,
        const std::string& instance_id) const;
    std::size_t pending_count_total() const;

private:
    using ChannelList = std::vector<std::shared_ptr<Channel>>;
    using InstanceChannelMap = std::unordered_map<std::string, ChannelList>;

    ChannelList& channels_for_legacy(
        const std::string& target_service,
        ChannelError* error);
    ChannelList& channels_for_instance(
        const ServiceInstance& instance,
        ChannelError* error);
    std::vector<ServiceInstance> instances_with_pending(
        const std::string& target_service) const;
    std::size_t pending_count(ChannelList& channels) const;
    std::size_t pending_count(const ChannelList& channels) const;
    std::size_t pending_count_unlocked(const std::string& target_service) const;
    std::shared_ptr<Channel> pick_channel(
        ChannelList& channels,
        const mmo::common::Envelope& request);
    RouteSelectionContext make_route_selection_context(
        const std::string& target_service,
        const mmo::common::Envelope& request,
        const ChannelCallOptions& options) const;
    static bool should_mark_unhealthy(ChannelErrorCode code);

    std::shared_ptr<EndpointResolver> resolver_;
    std::shared_ptr<ServiceRegistry> service_registry_;
    runtime::transport::TransportOptions transport_options_;
    ChannelConnectionPoolOptions options_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ChannelList> channels_;
    std::unordered_map<std::string, InstanceChannelMap> instance_channels_;
    ServiceInstanceSelector selector_;
};

}  // namespace runtime::channel

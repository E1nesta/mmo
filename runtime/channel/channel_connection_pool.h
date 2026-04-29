#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/channel/channel.h"
#include "runtime/channel/endpoint_resolver.h"
#include "runtime/foundation/server_config.h"

namespace mmo::runtime::channel {

struct ChannelConnectionPoolOptions {
    int connect_timeout_millis{};
    int request_timeout_millis{};
    std::size_t connections_per_upstream{1};
    std::size_t max_pending_requests_per_connection{128};
    std::size_t max_pending_requests_per_upstream{256};
};

ChannelConnectionPoolOptions make_channel_connection_pool_options(
    const mmo::runtime::foundation::ChannelConfig& config);

class ChannelConnectionPool {
public:
    ChannelConnectionPool(
        std::shared_ptr<EndpointResolver> resolver,
        mmo::runtime::transport::TransportOptions transport_options,
        ChannelConnectionPoolOptions options);

    ChannelResult call(
        const std::string& target_service,
        const mmo::common::Envelope& request,
        const ChannelCallOptions& options);

    std::size_t pending_count(const std::string& target_service) const;
    std::size_t pending_count_total() const;

private:
    using ChannelList = std::vector<std::shared_ptr<Channel>>;

    ChannelList& channels_for(
        const std::string& target_service,
        ChannelError* error);
    std::size_t pending_count(ChannelList& channels) const;
    std::shared_ptr<Channel> pick_channel(
        ChannelList& channels,
        const mmo::common::Envelope& request);

    std::shared_ptr<EndpointResolver> resolver_;
    mmo::runtime::transport::TransportOptions transport_options_;
    ChannelConnectionPoolOptions options_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, ChannelList> channels_;
};

}  // namespace mmo::runtime::channel

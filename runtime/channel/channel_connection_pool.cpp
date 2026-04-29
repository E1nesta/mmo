#include "runtime/channel/channel_connection_pool.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace mmo::runtime::channel {

ChannelConnectionPoolOptions make_channel_connection_pool_options(
    const mmo::runtime::foundation::ChannelConfig& config) {
    ChannelConnectionPoolOptions options;
    options.connect_timeout_millis = config.connect_timeout_millis;
    options.request_timeout_millis = config.request_timeout_millis;
    options.connections_per_upstream =
        static_cast<std::size_t>(std::max(1, config.connections_per_upstream));
    options.max_pending_requests_per_connection =
        static_cast<std::size_t>(
            std::max(1, config.max_pending_requests_per_connection));
    options.max_pending_requests_per_upstream =
        static_cast<std::size_t>(
            std::max(1, config.max_pending_requests_per_upstream));
    return options;
}

ChannelConnectionPool::ChannelConnectionPool(
    std::shared_ptr<EndpointResolver> resolver,
    mmo::runtime::transport::TransportOptions transport_options,
    ChannelConnectionPoolOptions options)
    : resolver_(std::move(resolver)),
      transport_options_(transport_options),
      options_(options) {
    options_.connections_per_upstream =
        std::max<std::size_t>(1, options_.connections_per_upstream);
    options_.max_pending_requests_per_connection =
        std::max<std::size_t>(1, options_.max_pending_requests_per_connection);
    options_.max_pending_requests_per_upstream =
        std::max<std::size_t>(1, options_.max_pending_requests_per_upstream);
}

ChannelResult ChannelConnectionPool::call(
    const std::string& target_service,
    const mmo::common::Envelope& request,
    const ChannelCallOptions& options) {
    ChannelError error;
    std::shared_ptr<Channel> channel;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& channels = channels_for(target_service, &error);
        if (!error.ok()) {
            return ChannelResult::failure(std::move(error));
        }
        if (pending_count(channels) >= options_.max_pending_requests_per_upstream) {
            return ChannelResult::failure(make_channel_error(
                ChannelErrorCode::kPendingLimitExceeded,
                "channel upstream pending limit exceeded"));
        }
        channel = pick_channel(channels, request);
    }
    if (channel == nullptr) {
        return ChannelResult::failure(make_channel_error(
            ChannelErrorCode::kEndpointNotFound, "channel is not available"));
    }
    return channel->call(request, options);
}

std::size_t ChannelConnectionPool::pending_count(
    const std::string& target_service) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = channels_.find(target_service);
    if (it == channels_.end()) {
        return 0;
    }

    std::size_t total = 0;
    for (const auto& channel : it->second) {
        total += channel->pending_count();
    }
    return total;
}

std::size_t ChannelConnectionPool::pending_count_total() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t total = 0;
    for (const auto& [service_name, channels] : channels_) {
        (void)service_name;
        for (const auto& channel : channels) {
            total += channel->pending_count();
        }
    }
    return total;
}

ChannelConnectionPool::ChannelList& ChannelConnectionPool::channels_for(
    const std::string& target_service,
    ChannelError* error) {
    auto it = channels_.find(target_service);
    if (it != channels_.end()) {
        return it->second;
    }

    static ChannelList empty_channels;
    const auto endpoint = resolver_ != nullptr
        ? resolver_->resolve(target_service)
        : std::nullopt;
    if (!endpoint.has_value()) {
        if (error != nullptr) {
            *error = make_channel_error(
                ChannelErrorCode::kEndpointNotFound,
                "missing channel endpoint: " + target_service);
        }
        return empty_channels;
    }

    ChannelList channels;
    channels.reserve(options_.connections_per_upstream);
    ChannelOptions channel_options;
    channel_options.connect_timeout_millis = options_.connect_timeout_millis;
    channel_options.request_timeout_millis = options_.request_timeout_millis;
    channel_options.max_pending_requests =
        options_.max_pending_requests_per_connection;
    for (std::size_t index = 0; index < options_.connections_per_upstream; ++index) {
        channels.push_back(std::make_shared<Channel>(
            *endpoint, transport_options_, channel_options));
    }

    auto inserted = channels_.emplace(target_service, std::move(channels));
    if (error != nullptr) {
        *error = ChannelError{};
    }
    return inserted.first->second;
}

std::shared_ptr<Channel> ChannelConnectionPool::pick_channel(
    ChannelList& channels,
    const mmo::common::Envelope& request) {
    if (channels.empty()) {
        return nullptr;
    }
    const auto key = request.player_id() > 0
        ? static_cast<std::uint64_t>(request.player_id())
        : request.request_id();
    return channels[key % channels.size()];
}

std::size_t ChannelConnectionPool::pending_count(ChannelList& channels) const {
    std::size_t total = 0;
    for (const auto& channel : channels) {
        total += channel->pending_count();
    }
    return total;
}

}  // namespace mmo::runtime::channel

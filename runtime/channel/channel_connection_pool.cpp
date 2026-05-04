#include "runtime/channel/channel_connection_pool.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <utility>

namespace runtime::channel {

ChannelConnectionPoolOptions make_channel_connection_pool_options(
    const runtime::foundation::ChannelConfig& config) {
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
    runtime::transport::TransportOptions transport_options,
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

ChannelConnectionPool::ChannelConnectionPool(
    std::shared_ptr<ServiceRegistry> service_registry,
    runtime::transport::TransportOptions transport_options,
    ChannelConnectionPoolOptions options)
    : service_registry_(std::move(service_registry)),
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
    std::optional<ServiceInstance> selected_instance;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (service_registry_ != nullptr) {
            if (pending_count_unlocked(target_service) >=
                options_.max_pending_requests_per_upstream) {
                return ChannelResult::failure(make_channel_error(
                    ChannelErrorCode::kPendingLimitExceeded,
                    "channel upstream pending limit exceeded"));
            }
            auto context =
                make_route_selection_context(target_service, request, options);
            context.max_pending_requests_per_instance =
                options_.max_pending_requests_per_upstream;
            auto result = selector_.select(
                instances_with_pending(target_service), context);
            if (!result.ok()) {
                return ChannelResult::failure(std::move(result.error));
            }
            selected_instance = result.instance;
            auto& channels = channels_for_instance(*selected_instance, &error);
            if (!error.ok()) {
                return ChannelResult::failure(std::move(error));
            }
            channel = pick_channel(channels, request);
        } else {
            auto& channels = channels_for_legacy(target_service, &error);
            if (!error.ok()) {
                return ChannelResult::failure(std::move(error));
            }
            if (pending_count(channels) >=
                options_.max_pending_requests_per_upstream) {
                return ChannelResult::failure(make_channel_error(
                    ChannelErrorCode::kPendingLimitExceeded,
                    "channel upstream pending limit exceeded"));
            }
            channel = pick_channel(channels, request);
        }
    }
    if (channel == nullptr) {
        return ChannelResult::failure(make_channel_error(
            ChannelErrorCode::kEndpointNotFound, "channel is not available"));
    }
    auto result = channel->call(request, options);
    if (selected_instance.has_value() && service_registry_ != nullptr &&
        !result.ok() && should_mark_unhealthy(result.error().code)) {
        service_registry_->mark_unhealthy(
            selected_instance->service_name,
            selected_instance->instance_id,
            result.error().message,
            std::chrono::milliseconds(1000));
    }
    return result;
}

std::size_t ChannelConnectionPool::pending_count(
    const std::string& target_service) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_count_unlocked(target_service);
}

std::size_t ChannelConnectionPool::pending_count_unlocked(
    const std::string& target_service) const {
    std::size_t total = 0;
    const auto legacy = channels_.find(target_service);
    if (legacy != channels_.end()) {
        for (const auto& channel : legacy->second) {
            total += channel->pending_count();
        }
    }
    const auto instances = instance_channels_.find(target_service);
    if (instances != instance_channels_.end()) {
        for (const auto& [instance_id, channels] : instances->second) {
            (void)instance_id;
            total += pending_count(channels);
        }
    }
    return total;
}

std::size_t ChannelConnectionPool::pending_count(
    const std::string& target_service,
    const std::string& instance_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto service_it = instance_channels_.find(target_service);
    if (service_it == instance_channels_.end()) {
        return 0;
    }
    const auto instance_it = service_it->second.find(instance_id);
    if (instance_it == service_it->second.end()) {
        return 0;
    }
    return pending_count(instance_it->second);
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
    for (const auto& [service_name, instance_channels] : instance_channels_) {
        (void)service_name;
        for (const auto& [instance_id, channels] : instance_channels) {
            (void)instance_id;
            total += pending_count(channels);
        }
    }
    return total;
}

ChannelConnectionPool::ChannelList& ChannelConnectionPool::channels_for_legacy(
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

ChannelConnectionPool::ChannelList& ChannelConnectionPool::channels_for_instance(
    const ServiceInstance& instance,
    ChannelError* error) {
    auto& service_channels = instance_channels_[instance.service_name];
    auto it = service_channels.find(instance.instance_id);
    if (it != service_channels.end()) {
        return it->second;
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
            instance.endpoint, transport_options_, channel_options));
    }

    auto inserted =
        service_channels.emplace(instance.instance_id, std::move(channels));
    if (error != nullptr) {
        *error = ChannelError{};
    }
    return inserted.first->second;
}

std::vector<ServiceInstance> ChannelConnectionPool::instances_with_pending(
    const std::string& target_service) const {
    if (service_registry_ == nullptr) {
        return {};
    }
    auto instances = service_registry_->list_instances(target_service);
    const auto service_it = instance_channels_.find(target_service);
    if (service_it == instance_channels_.end()) {
        return instances;
    }
    for (auto& instance : instances) {
        const auto instance_it = service_it->second.find(instance.instance_id);
        if (instance_it != service_it->second.end()) {
            instance.pending_count = pending_count(instance_it->second);
        }
    }
    return instances;
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
    return pending_count(static_cast<const ChannelList&>(channels));
}

std::size_t ChannelConnectionPool::pending_count(
    const ChannelList& channels) const {
    std::size_t total = 0;
    for (const auto& channel : channels) {
        total += channel->pending_count();
    }
    return total;
}

RouteSelectionContext ChannelConnectionPool::make_route_selection_context(
    const std::string& target_service,
    const mmo::common::Envelope& request,
    const ChannelCallOptions& options) const {
    RouteSelectionContext context;
    context.target_service = target_service;
    context.policy = options.routing_policy;
    context.player_id = request.player_id();
    context.request_id = request.request_id();
    context.message_type = request.message_type();
    context.route_key = options.route_key;
    context.target_instance_id = options.target_instance_id;
    return context;
}

bool ChannelConnectionPool::should_mark_unhealthy(ChannelErrorCode code) {
    switch (code) {
        case ChannelErrorCode::kConnectFailed:
        case ChannelErrorCode::kReadFailed:
        case ChannelErrorCode::kTimeout:
        case ChannelErrorCode::kChannelClosed:
            return true;
        case ChannelErrorCode::kOk:
        case ChannelErrorCode::kWriteFailed:
        case ChannelErrorCode::kEncodeFailed:
        case ChannelErrorCode::kDecodeFailed:
        case ChannelErrorCode::kRemoteError:
        case ChannelErrorCode::kRequestIdMismatch:
        case ChannelErrorCode::kEndpointNotFound:
        case ChannelErrorCode::kPendingLimitExceeded:
        case ChannelErrorCode::kDuplicateRequestId:
            return false;
    }
    return false;
}

}  // namespace runtime::channel

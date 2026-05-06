#include "runtime/rpc/rpc_connection_pool.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <utility>

namespace runtime::rpc {

RpcConnectionPoolOptions make_rpc_connection_pool_options(
    const runtime::foundation::RpcConfig& config) {
    RpcConnectionPoolOptions options;
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

RpcConnectionPool::RpcConnectionPool(
    std::shared_ptr<RpcServiceRegistry> service_registry,
    runtime::net::TransportOptions transport_options,
    RpcConnectionPoolOptions options)
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

RpcResult RpcConnectionPool::call(
    const std::string& target_service,
    const runtime::protocol::FrameMessage& request,
    const RpcCallOptions& options) {
    RpcError error;
    std::shared_ptr<RpcConnection> rpc_connection;
    std::optional<RpcServiceInstance> selected_instance;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (service_registry_ == nullptr) {
            return RpcResult::failure(make_rpc_error(
                RpcErrorCode::kEndpointNotFound,
                "rpc service registry is not configured"));
        }
        if (pending_count_unlocked(target_service) >=
            options_.max_pending_requests_per_upstream) {
            return RpcResult::failure(make_rpc_error(
                RpcErrorCode::kPendingLimitExceeded,
                "rpc upstream pending limit exceeded"));
        }

        auto context = make_route_selection_context(target_service, request, options);
        context.max_pending_requests_per_instance =
            options_.max_pending_requests_per_upstream;
        auto route = selector_.select(instances_with_pending(target_service), context);
        if (!route.ok()) {
            return RpcResult::failure(std::move(route.error));
        }

        selected_instance = route.instance;
        auto& rpc_connections =
            connections_for_service_instance(*selected_instance, &error);
        if (!error.ok()) {
            return RpcResult::failure(std::move(error));
        }
        rpc_connection = pick_connection(rpc_connections, request);
    }
    if (rpc_connection == nullptr) {
        return RpcResult::failure(make_rpc_error(
            RpcErrorCode::kEndpointNotFound, "rpc connection is not available"));
    }
    auto result = rpc_connection->call(request, options);
    if (selected_instance.has_value() &&
        !result.ok() &&
        should_mark_unhealthy(result.error().code)) {
        service_registry_->mark_unhealthy(
            selected_instance->service_name,
            selected_instance->instance_id,
            result.error().message,
            std::chrono::milliseconds(1000));
    }
    return result;
}

RpcResult RpcConnectionPool::cast(
    const std::string& target_service,
    const runtime::protocol::FrameMessage& request,
    const RpcCallOptions& options) {
    RpcError error;
    std::shared_ptr<RpcConnection> rpc_connection;
    std::optional<RpcServiceInstance> selected_instance;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (service_registry_ == nullptr) {
            return RpcResult::failure(make_rpc_error(
                RpcErrorCode::kEndpointNotFound,
                "rpc service registry is not configured"));
        }

        auto context = make_route_selection_context(target_service, request, options);
        auto route = selector_.select(instances_with_pending(target_service), context);
        if (!route.ok()) {
            return RpcResult::failure(std::move(route.error));
        }

        selected_instance = route.instance;
        auto& rpc_connections =
            connections_for_service_instance(*selected_instance, &error);
        if (!error.ok()) {
            return RpcResult::failure(std::move(error));
        }
        rpc_connection = pick_connection(rpc_connections, request);
    }
    if (rpc_connection == nullptr) {
        return RpcResult::failure(make_rpc_error(
            RpcErrorCode::kEndpointNotFound, "rpc connection is not available"));
    }
    auto result = rpc_connection->cast(request, options);
    if (selected_instance.has_value() &&
        !result.ok() &&
        should_mark_unhealthy(result.error().code)) {
        service_registry_->mark_unhealthy(
            selected_instance->service_name,
            selected_instance->instance_id,
            result.error().message,
            std::chrono::milliseconds(1000));
    }
    return result;
}

std::size_t RpcConnectionPool::pending_count(
    const std::string& target_service) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_count_unlocked(target_service);
}

std::size_t RpcConnectionPool::pending_count_unlocked(
    const std::string& target_service) const {
    std::size_t total = 0;
    const auto instances = instance_connections_.find(target_service);
    if (instances != instance_connections_.end()) {
        for (const auto& [instance_id, rpc_connections] : instances->second) {
            (void)instance_id;
            total += pending_count(rpc_connections);
        }
    }
    return total;
}

std::size_t RpcConnectionPool::pending_count(
    const std::string& target_service,
    const std::string& instance_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto service_it = instance_connections_.find(target_service);
    if (service_it == instance_connections_.end()) {
        return 0;
    }
    const auto instance_it = service_it->second.find(instance_id);
    if (instance_it == service_it->second.end()) {
        return 0;
    }
    return pending_count(instance_it->second);
}

std::size_t RpcConnectionPool::pending_count_total() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t total = 0;
    for (const auto& [service_name, instance_connections] : instance_connections_) {
        (void)service_name;
        for (const auto& [instance_id, rpc_connections] : instance_connections) {
            (void)instance_id;
            total += pending_count(rpc_connections);
        }
    }
    return total;
}

RpcConnectionPool::RpcConnectionList&
RpcConnectionPool::connections_for_service_instance(
    const RpcServiceInstance& instance,
    RpcError* error) {
    auto& service_instance_connections =
        instance_connections_[instance.service_name];
    auto it = service_instance_connections.find(instance.instance_id);
    if (it != service_instance_connections.end()) {
        return it->second;
    }

    RpcConnectionList rpc_connections;
    rpc_connections.reserve(options_.connections_per_upstream);
    RpcConnectionOptions rpc_connection_options;
    rpc_connection_options.connect_timeout_millis = options_.connect_timeout_millis;
    rpc_connection_options.request_timeout_millis = options_.request_timeout_millis;
    rpc_connection_options.max_pending_requests =
        options_.max_pending_requests_per_connection;
    for (std::size_t index = 0; index < options_.connections_per_upstream; ++index) {
        rpc_connections.push_back(std::make_shared<RpcConnection>(
            instance.endpoint, transport_options_, rpc_connection_options));
    }

    auto inserted =
        service_instance_connections.emplace(instance.instance_id, std::move(rpc_connections));
    if (error != nullptr) {
        *error = RpcError{};
    }
    return inserted.first->second;
}

std::vector<RpcServiceInstance> RpcConnectionPool::instances_with_pending(
    const std::string& target_service) const {
    if (service_registry_ == nullptr) {
        return {};
    }
    auto instances = service_registry_->list_instances(target_service);
    const auto service_it = instance_connections_.find(target_service);
    if (service_it == instance_connections_.end()) {
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

std::shared_ptr<RpcConnection> RpcConnectionPool::pick_connection(
    RpcConnectionList& rpc_connections,
    const runtime::protocol::FrameMessage& request) {
    if (rpc_connections.empty()) {
        return nullptr;
    }
    const auto key = request.route_key() != 0
        ? request.route_key()
        : request.request_id();
    return rpc_connections[key % rpc_connections.size()];
}

std::size_t RpcConnectionPool::pending_count(RpcConnectionList& rpc_connections) const {
    return pending_count(static_cast<const RpcConnectionList&>(rpc_connections));
}

std::size_t RpcConnectionPool::pending_count(
    const RpcConnectionList& rpc_connections) const {
    std::size_t total = 0;
    for (const auto& rpc_connection : rpc_connections) {
        total += rpc_connection->pending_count();
    }
    return total;
}

RpcRouteSelectionContext RpcConnectionPool::make_route_selection_context(
    const std::string& target_service,
    const runtime::protocol::FrameMessage& request,
    const RpcCallOptions& options) const {
    RpcRouteSelectionContext context;
    context.target_service = target_service;
    context.policy = options.routing_policy;
    context.request_id = request.request_id();
    context.route_key = options.route_key != 0 ? options.route_key : request.route_key();
    context.target_instance_id = options.target_instance_id;
    return context;
}

bool RpcConnectionPool::should_mark_unhealthy(RpcErrorCode code) {
    switch (code) {
        case RpcErrorCode::kConnectFailed:
        case RpcErrorCode::kReadFailed:
        case RpcErrorCode::kTimeout:
        case RpcErrorCode::kConnectionClosed:
            return true;
        case RpcErrorCode::kOk:
        case RpcErrorCode::kWriteFailed:
        case RpcErrorCode::kEncodeFailed:
        case RpcErrorCode::kDecodeFailed:
        case RpcErrorCode::kRemoteError:
        case RpcErrorCode::kRequestIdMismatch:
        case RpcErrorCode::kEndpointNotFound:
        case RpcErrorCode::kPendingLimitExceeded:
        case RpcErrorCode::kDuplicateRequestId:
            return false;
    }
    return false;
}

}  // namespace runtime::rpc

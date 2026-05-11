#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "runtime/protocol/frame.h"
#include "runtime/rpc/rpc_connection.h"
#include "runtime/rpc/rpc_options.h"
#include "runtime/rpc/rpc_routing_policy.h"
#include "runtime/rpc/rpc_service_registry.h"
#include "runtime/scheduler/io_context_pool.h"

namespace runtime::rpc {

struct RpcConnectionPoolOptions {
    int connect_timeout_millis{};
    int request_timeout_millis{};
    std::size_t connections_per_upstream{1};
    std::size_t max_pending_requests_per_connection{128};
    std::size_t max_pending_requests_per_upstream{256};
};

class RpcConnectionPool {
public:
    RpcConnectionPool(
        std::shared_ptr<RpcServiceRegistry> service_registry,
        runtime::net::TransportOptions transport_options,
        RpcConnectionPoolOptions options);

    RpcResult call(
        const std::string& target_service,
        const runtime::protocol::FrameMessage& request,
        const RpcOptions& options);
    RpcError call_async(
        const std::string& target_service,
        const runtime::protocol::FrameMessage& request,
        const RpcOptions& options,
        RpcResultHandler handler);
    RpcResult cast(
        const std::string& target_service,
        const runtime::protocol::FrameMessage& request,
        const RpcOptions& options);

    std::size_t pending_count(const std::string& target_service) const;
    std::size_t pending_count(
        const std::string& target_service,
        const std::string& instance_id) const;
    std::size_t pending_count_total() const;

private:
    using RpcConnectionList = std::vector<std::shared_ptr<RpcConnection>>;
    using RpcInstanceConnectionMap =
        std::unordered_map<std::string, RpcConnectionList>;

    RpcConnectionList& connections_for_service_instance(
        const RpcServiceInstance& instance,
        RpcError* error);
    std::vector<RpcServiceInstance> instances_with_pending(
        const std::string& target_service) const;
    std::size_t pending_count(RpcConnectionList& rpc_connections) const;
    std::size_t pending_count(const RpcConnectionList& rpc_connections) const;
    std::size_t pending_count_unlocked(const std::string& target_service) const;
    std::shared_ptr<RpcConnection> pick_connection(
        RpcConnectionList& rpc_connections,
        const runtime::protocol::FrameMessage& request);
    RpcRouteSelectionContext make_route_selection_context(
        const std::string& target_service,
        const runtime::protocol::FrameMessage& request,
        const RpcOptions& options) const;
    static bool should_mark_unhealthy(RpcErrorCode code);

    std::shared_ptr<RpcServiceRegistry> service_registry_;
    runtime::net::TransportOptions transport_options_;
    RpcConnectionPoolOptions options_;
    std::shared_ptr<runtime::scheduler::IOContextPool> io_context_pool_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, RpcInstanceConnectionMap> instance_connections_;
    RpcServiceInstanceSelector selector_;
};

}  // namespace runtime::rpc

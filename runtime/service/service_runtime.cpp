#include "runtime/service/service_runtime.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "runtime/rpc/rpc_connection_pool.h"
#include "runtime/rpc/rpc_service_registry.h"
#include "runtime/observability/logging.h"
#include "runtime/storage/storage_runtime.h"
#include "runtime/net/rpc_frame_transport.h"

namespace runtime::service {
namespace {

runtime::net::TransportOptions make_transport_options(
    const runtime::foundation::TcpTransportConfig& tcp_config,
    const runtime::foundation::SchedulerConfig& scheduler_config) {
    runtime::net::TransportOptions options;
    options.max_payload_bytes = tcp_config.max_frame_payload_bytes > 0
        ? static_cast<std::uint32_t>(tcp_config.max_frame_payload_bytes)
        : runtime::net::kDefaultMaxFramePayloadBytes;
    options.timeout_millis = tcp_config.timeout_millis > 0
        ? tcp_config.timeout_millis
        : runtime::net::kDefaultTransportTimeoutMillis;
    options.listen_backlog = tcp_config.listen_backlog > 0
        ? tcp_config.listen_backlog
        : options.listen_backlog;
    options.max_connections = tcp_config.max_connections > 0
        ? static_cast<std::uint32_t>(tcp_config.max_connections)
        : options.max_connections;
    options.max_write_queue_depth = tcp_config.max_write_queue_depth > 0
        ? static_cast<std::uint32_t>(tcp_config.max_write_queue_depth)
        : options.max_write_queue_depth;
    options.max_inflight_frames_per_connection =
        tcp_config.max_inflight_frames_per_connection > 0
            ? static_cast<std::uint32_t>(
                  tcp_config.max_inflight_frames_per_connection)
            : options.max_inflight_frames_per_connection;
    options.io_thread_count = scheduler_config.io_threads > 0
        ? static_cast<std::uint32_t>(scheduler_config.io_threads)
        : runtime::net::kDefaultIoThreadCount;
    options.handler_shard_count = scheduler_config.handler_shards > 0
        ? static_cast<std::uint32_t>(scheduler_config.handler_shards)
        : runtime::net::kDefaultHandlerShardCount;
    options.max_handler_queue_depth =
        scheduler_config.max_handler_queue_depth_per_shard > 0
            ? static_cast<std::uint32_t>(
                  scheduler_config.max_handler_queue_depth_per_shard)
            : runtime::net::kDefaultMaxHandlerQueueDepth;
    return options;
}

runtime::rpc::RpcConnectionPoolOptions make_rpc_connection_pool_options(
    const runtime::foundation::RpcConfig& config) {
    runtime::rpc::RpcConnectionPoolOptions options;
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

std::vector<runtime::rpc::RpcServiceInstance> make_rpc_service_instances(
    const runtime::foundation::ServerConfig& config) {
    std::vector<runtime::rpc::RpcServiceInstance> instances;
    for (const auto& [service_name, service_config] : config.services) {
        instances.reserve(instances.size() + service_config.instances.size());
        for (const auto& instance_config : service_config.instances) {
            runtime::rpc::RpcServiceInstance instance;
            instance.service_name = service_name;
            instance.instance_id = instance_config.instance_id;
            instance.endpoint.host = instance_config.host;
            instance.endpoint.port = instance_config.tcp_port;
            instance.zone = instance_config.zone;
            instance.weight = instance_config.weight;
            instance.state = runtime::rpc::parse_service_instance_state(
                instance_config.state);
            instance.metadata = instance_config.metadata;
            instances.push_back(std::move(instance));
        }
    }
    return instances;
}

}  // namespace

runtime::net::TransportOptions make_server_transport_options(
    const ServiceApp& app) {
    return make_transport_options(
        app.config().transport.tcp,
        app.config().scheduler);
}

runtime::scheduler::ShardedExecutorOptions make_entity_scheduler_options(
    const ServiceApp& app) {
    runtime::scheduler::ShardedExecutorOptions options;
    options.shard_count = static_cast<std::size_t>(
        std::max(1, app.config().scheduler.handler_shards));
    options.max_queue_depth_per_shard = static_cast<std::size_t>(
        std::max(1, app.config().scheduler.max_handler_queue_depth_per_shard));
    return options;
}

std::unique_ptr<runtime::rpc::RpcClient> make_rpc_client(
    const ServiceApp& app,
    const runtime::net::TransportOptions& transport_options) {
    auto service_registry =
        std::make_shared<runtime::rpc::StaticRpcServiceRegistry>(
            make_rpc_service_instances(app.config()));
    runtime::rpc::RpcClientOptions options;
    options.source_service = app.service_name();
    return std::make_unique<runtime::rpc::RpcClient>(
        service_registry,
        transport_options,
        make_rpc_connection_pool_options(app.config().rpc),
        std::move(options));
}

std::shared_ptr<runtime::storage::MysqlConnectionPool> require_mysql_pool(
    const ServiceApp& app) {
    std::shared_ptr<runtime::storage::MysqlConnectionPool> pool;
    std::string error_message;
    if (!runtime::storage::initialize_mysql_pool(
            app.config(), &pool, &error_message)) {
        runtime::observability::log_error(
            runtime::observability::LogContext{app.service_name()},
            "mysql_pool_init_failed error=" + error_message);
        return nullptr;
    }
    return pool;
}

std::shared_ptr<runtime::storage::RedisConnectionPool> require_redis_pool(
    const ServiceApp& app) {
    std::shared_ptr<runtime::storage::RedisConnectionPool> pool;
    std::string error_message;
    if (!runtime::storage::initialize_redis_pool(
            app.config(), &pool, &error_message)) {
        runtime::observability::log_error(
            runtime::observability::LogContext{app.service_name()},
            "redis_pool_init_failed error=" + error_message);
        return nullptr;
    }
    return pool;
}

int run_rpc_service(
    const ServiceApp& app,
    const runtime::rpc::RpcDispatcher& dispatcher,
    const runtime::net::TransportOptions& transport_options) {
    runtime::net::RpcFrameTransport transport(
        app.service_config().tcp_port,
        dispatcher.handler(),
        app.service_name(),
        transport_options);

    runtime::observability::log_info(
        runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return transport.run();
}

}  // namespace runtime::service

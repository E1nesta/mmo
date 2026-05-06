#include "runtime/observability/metrics.h"

namespace runtime::observability {
namespace {

std::string upstream_key(
    const std::string& service,
    const std::string& instance_id) {
    return service + "/" + instance_id;
}

UpstreamInstanceMetrics& upstream_metrics(
    std::unordered_map<std::string, UpstreamInstanceMetrics>& instances,
    const std::string& service,
    const std::string& instance_id) {
    auto& metrics = instances[upstream_key(service, instance_id)];
    metrics.service = service;
    metrics.instance_id = instance_id;
    return metrics;
}

}  // namespace

void MetricsRegistry::record_connection_open() {
    active_connections_.fetch_add(1, std::memory_order_relaxed);
    accepted_connections_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_connection_close() {
    active_connections_.fetch_sub(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_request() {
    requests_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_error() {
    errors_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_bytes_read(std::uint64_t bytes) {
    bytes_read_total_.fetch_add(bytes, std::memory_order_relaxed);
}

void MetricsRegistry::record_bytes_written(std::uint64_t bytes) {
    bytes_written_total_.fetch_add(bytes, std::memory_order_relaxed);
}

void MetricsRegistry::record_handler_latency_ms(std::uint64_t latency_ms) {
    handler_latency_ms_total_.fetch_add(latency_ms, std::memory_order_relaxed);
}

void MetricsRegistry::set_executor_queue_depth(std::uint64_t queue_depth) {
    executor_queue_depth_.store(queue_depth, std::memory_order_relaxed);
}

void MetricsRegistry::record_executor_post_failed() {
    executor_post_failed_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_executor_queue_overflow() {
    executor_queue_overflow_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_handler_rejected() {
    handler_rejected_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::set_rpc_pending_count(std::uint64_t pending_count) {
    rpc_pending_count_.store(pending_count, std::memory_order_relaxed);
}

void MetricsRegistry::record_rpc_call() {
    rpc_calls_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_rpc_error() {
    rpc_errors_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_rpc_timeout() {
    rpc_timeout_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_rpc_remote_error() {
    rpc_remote_error_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_login_success() {
    login_success_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_login_failed() {
    login_failed_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_gateway_ticket_issued() {
    gateway_ticket_issued_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_gateway_ticket_rejected() {
    gateway_ticket_rejected_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_gateway_ticket_replay() {
    gateway_ticket_replay_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_gateway_login_success() {
    gateway_login_success_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_gateway_login_failed() {
    gateway_login_failed_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_game_session_expired() {
    game_session_expired_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_reconnect_success() {
    reconnect_success_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_reconnect_failed() {
    reconnect_failed_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::set_upstream_instance_pending(
    const std::string& service,
    const std::string& instance_id,
    std::uint64_t pending) {
    std::lock_guard<std::mutex> lock(upstream_mutex_);
    upstream_metrics(upstream_instances_, service, instance_id).pending = pending;
}

void MetricsRegistry::set_upstream_instance_healthy(
    const std::string& service,
    const std::string& instance_id,
    bool healthy) {
    std::lock_guard<std::mutex> lock(upstream_mutex_);
    upstream_metrics(upstream_instances_, service, instance_id).healthy =
        healthy ? 1U : 0U;
}

void MetricsRegistry::record_upstream_instance_unhealthy(
    const std::string& service,
    const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(upstream_mutex_);
    ++upstream_metrics(upstream_instances_, service, instance_id).unhealthy_total;
}

void MetricsRegistry::record_upstream_instance_recovered(
    const std::string& service,
    const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(upstream_mutex_);
    ++upstream_metrics(upstream_instances_, service, instance_id).recovered_total;
}

void MetricsRegistry::record_upstream_instance_failover(
    const std::string& service,
    const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(upstream_mutex_);
    ++upstream_metrics(upstream_instances_, service, instance_id).failover_total;
}

void MetricsRegistry::record_upstream_instance_circuit_open(
    const std::string& service,
    const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(upstream_mutex_);
    ++upstream_metrics(upstream_instances_, service, instance_id).circuit_open_total;
}

void MetricsRegistry::record_upstream_instance_request_timeout(
    const std::string& service,
    const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(upstream_mutex_);
    ++upstream_metrics(upstream_instances_, service, instance_id)
          .request_timeout_total;
}

void MetricsRegistry::record_upstream_instance_remote_error(
    const std::string& service,
    const std::string& instance_id) {
    std::lock_guard<std::mutex> lock(upstream_mutex_);
    ++upstream_metrics(upstream_instances_, service, instance_id)
          .remote_error_total;
}

MetricsSnapshot MetricsRegistry::snapshot() const {
    MetricsSnapshot snapshot;
    snapshot.active_connections =
        active_connections_.load(std::memory_order_relaxed);
    snapshot.accepted_connections =
        accepted_connections_.load(std::memory_order_relaxed);
    snapshot.requests_total = requests_total_.load(std::memory_order_relaxed);
    snapshot.errors_total = errors_total_.load(std::memory_order_relaxed);
    snapshot.bytes_read_total = bytes_read_total_.load(std::memory_order_relaxed);
    snapshot.bytes_written_total =
        bytes_written_total_.load(std::memory_order_relaxed);
    snapshot.handler_latency_ms_total =
        handler_latency_ms_total_.load(std::memory_order_relaxed);
    snapshot.executor_queue_depth =
        executor_queue_depth_.load(std::memory_order_relaxed);
    snapshot.executor_post_failed_total =
        executor_post_failed_total_.load(std::memory_order_relaxed);
    snapshot.executor_queue_overflow_total =
        executor_queue_overflow_total_.load(std::memory_order_relaxed);
    snapshot.handler_rejected_total =
        handler_rejected_total_.load(std::memory_order_relaxed);
    snapshot.rpc_pending_count =
        rpc_pending_count_.load(std::memory_order_relaxed);
    snapshot.rpc_calls_total =
        rpc_calls_total_.load(std::memory_order_relaxed);
    snapshot.rpc_errors_total =
        rpc_errors_total_.load(std::memory_order_relaxed);
    snapshot.rpc_timeout_total =
        rpc_timeout_total_.load(std::memory_order_relaxed);
    snapshot.rpc_remote_error_total =
        rpc_remote_error_total_.load(std::memory_order_relaxed);
    snapshot.login_success_total =
        login_success_total_.load(std::memory_order_relaxed);
    snapshot.login_failed_total =
        login_failed_total_.load(std::memory_order_relaxed);
    snapshot.gateway_ticket_issued_total =
        gateway_ticket_issued_total_.load(std::memory_order_relaxed);
    snapshot.gateway_ticket_rejected_total =
        gateway_ticket_rejected_total_.load(std::memory_order_relaxed);
    snapshot.gateway_ticket_replay_total =
        gateway_ticket_replay_total_.load(std::memory_order_relaxed);
    snapshot.gateway_login_success_total =
        gateway_login_success_total_.load(std::memory_order_relaxed);
    snapshot.gateway_login_failed_total =
        gateway_login_failed_total_.load(std::memory_order_relaxed);
    snapshot.game_session_expired_total =
        game_session_expired_total_.load(std::memory_order_relaxed);
    snapshot.reconnect_success_total =
        reconnect_success_total_.load(std::memory_order_relaxed);
    snapshot.reconnect_failed_total =
        reconnect_failed_total_.load(std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(upstream_mutex_);
        snapshot.upstream_instances.reserve(upstream_instances_.size());
        for (const auto& [key, metrics] : upstream_instances_) {
            (void)key;
            snapshot.upstream_instances.push_back(metrics);
        }
    }
    return snapshot;
}

}  // namespace runtime::observability

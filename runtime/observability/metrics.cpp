#include "runtime/observability/metrics.h"

namespace mmo::runtime::observability {

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

void MetricsRegistry::set_channel_pending_count(std::uint64_t pending_count) {
    channel_pending_count_.store(pending_count, std::memory_order_relaxed);
}

void MetricsRegistry::record_channel_request() {
    channel_requests_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_channel_error() {
    channel_errors_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::record_channel_pending_limit() {
    channel_pending_limit_total_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsRegistry::set_rpc_pending_count(std::uint64_t pending_count) {
    rpc_pending_count_.store(pending_count, std::memory_order_relaxed);
}

void MetricsRegistry::record_rpc_request() {
    rpc_requests_total_.fetch_add(1, std::memory_order_relaxed);
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
    snapshot.channel_pending_count =
        channel_pending_count_.load(std::memory_order_relaxed);
    snapshot.channel_requests_total =
        channel_requests_total_.load(std::memory_order_relaxed);
    snapshot.channel_errors_total =
        channel_errors_total_.load(std::memory_order_relaxed);
    snapshot.channel_pending_limit_total =
        channel_pending_limit_total_.load(std::memory_order_relaxed);
    snapshot.rpc_pending_count =
        rpc_pending_count_.load(std::memory_order_relaxed);
    snapshot.rpc_requests_total =
        rpc_requests_total_.load(std::memory_order_relaxed);
    snapshot.rpc_errors_total =
        rpc_errors_total_.load(std::memory_order_relaxed);
    snapshot.rpc_timeout_total =
        rpc_timeout_total_.load(std::memory_order_relaxed);
    snapshot.rpc_remote_error_total =
        rpc_remote_error_total_.load(std::memory_order_relaxed);
    return snapshot;
}

}  // namespace mmo::runtime::observability

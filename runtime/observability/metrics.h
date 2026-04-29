#pragma once

#include <atomic>
#include <cstdint>

namespace mmo::runtime::observability {

struct MetricsSnapshot {
    std::uint64_t active_connections{};
    std::uint64_t accepted_connections{};
    std::uint64_t requests_total{};
    std::uint64_t errors_total{};
    std::uint64_t bytes_read_total{};
    std::uint64_t bytes_written_total{};
    std::uint64_t handler_latency_ms_total{};
    std::uint64_t executor_queue_depth{};
    std::uint64_t executor_post_failed_total{};
    std::uint64_t executor_queue_overflow_total{};
    std::uint64_t handler_rejected_total{};
    std::uint64_t channel_pending_count{};
    std::uint64_t channel_requests_total{};
    std::uint64_t channel_errors_total{};
    std::uint64_t channel_pending_limit_total{};
    std::uint64_t rpc_pending_count{};
    std::uint64_t rpc_requests_total{};
    std::uint64_t rpc_errors_total{};
    std::uint64_t rpc_timeout_total{};
    std::uint64_t rpc_remote_error_total{};
};

class MetricsRegistry {
public:
    void record_connection_open();
    void record_connection_close();
    void record_request();
    void record_error();
    void record_bytes_read(std::uint64_t bytes);
    void record_bytes_written(std::uint64_t bytes);
    void record_handler_latency_ms(std::uint64_t latency_ms);
    void set_executor_queue_depth(std::uint64_t queue_depth);
    void record_executor_post_failed();
    void record_executor_queue_overflow();
    void record_handler_rejected();
    void set_channel_pending_count(std::uint64_t pending_count);
    void record_channel_request();
    void record_channel_error();
    void record_channel_pending_limit();
    void set_rpc_pending_count(std::uint64_t pending_count);
    void record_rpc_request();
    void record_rpc_error();
    void record_rpc_timeout();
    void record_rpc_remote_error();

    MetricsSnapshot snapshot() const;

private:
    std::atomic<std::uint64_t> active_connections_{};
    std::atomic<std::uint64_t> accepted_connections_{};
    std::atomic<std::uint64_t> requests_total_{};
    std::atomic<std::uint64_t> errors_total_{};
    std::atomic<std::uint64_t> bytes_read_total_{};
    std::atomic<std::uint64_t> bytes_written_total_{};
    std::atomic<std::uint64_t> handler_latency_ms_total_{};
    std::atomic<std::uint64_t> executor_queue_depth_{};
    std::atomic<std::uint64_t> executor_post_failed_total_{};
    std::atomic<std::uint64_t> executor_queue_overflow_total_{};
    std::atomic<std::uint64_t> handler_rejected_total_{};
    std::atomic<std::uint64_t> channel_pending_count_{};
    std::atomic<std::uint64_t> channel_requests_total_{};
    std::atomic<std::uint64_t> channel_errors_total_{};
    std::atomic<std::uint64_t> channel_pending_limit_total_{};
    std::atomic<std::uint64_t> rpc_pending_count_{};
    std::atomic<std::uint64_t> rpc_requests_total_{};
    std::atomic<std::uint64_t> rpc_errors_total_{};
    std::atomic<std::uint64_t> rpc_timeout_total_{};
    std::atomic<std::uint64_t> rpc_remote_error_total_{};
};

}  // namespace mmo::runtime::observability

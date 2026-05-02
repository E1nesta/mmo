#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace mmo::runtime::observability {

struct UpstreamInstanceMetrics {
    std::string service;
    std::string instance_id;
    std::uint64_t healthy{};
    std::uint64_t pending{};
    std::uint64_t unhealthy_total{};
    std::uint64_t recovered_total{};
    std::uint64_t failover_total{};
    std::uint64_t circuit_open_total{};
    std::uint64_t request_timeout_total{};
    std::uint64_t remote_error_total{};
};

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
    std::uint64_t login_success_total{};
    std::uint64_t login_failed_total{};
    std::uint64_t gateway_ticket_issued_total{};
    std::uint64_t gateway_ticket_rejected_total{};
    std::uint64_t gateway_ticket_replay_total{};
    std::uint64_t gate_login_success_total{};
    std::uint64_t gate_login_failed_total{};
    std::uint64_t game_session_expired_total{};
    std::uint64_t reconnect_success_total{};
    std::uint64_t reconnect_failed_total{};
    std::uint64_t internal_auth_failed_total{};
    std::vector<UpstreamInstanceMetrics> upstream_instances;
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
    void record_login_success();
    void record_login_failed();
    void record_gateway_ticket_issued();
    void record_gateway_ticket_rejected();
    void record_gateway_ticket_replay();
    void record_gate_login_success();
    void record_gate_login_failed();
    void record_game_session_expired();
    void record_reconnect_success();
    void record_reconnect_failed();
    void record_internal_auth_failed();
    void set_upstream_instance_pending(
        const std::string& service,
        const std::string& instance_id,
        std::uint64_t pending);
    void set_upstream_instance_healthy(
        const std::string& service,
        const std::string& instance_id,
        bool healthy);
    void record_upstream_instance_unhealthy(
        const std::string& service,
        const std::string& instance_id);
    void record_upstream_instance_recovered(
        const std::string& service,
        const std::string& instance_id);
    void record_upstream_instance_failover(
        const std::string& service,
        const std::string& instance_id);
    void record_upstream_instance_circuit_open(
        const std::string& service,
        const std::string& instance_id);
    void record_upstream_instance_request_timeout(
        const std::string& service,
        const std::string& instance_id);
    void record_upstream_instance_remote_error(
        const std::string& service,
        const std::string& instance_id);

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
    std::atomic<std::uint64_t> login_success_total_{};
    std::atomic<std::uint64_t> login_failed_total_{};
    std::atomic<std::uint64_t> gateway_ticket_issued_total_{};
    std::atomic<std::uint64_t> gateway_ticket_rejected_total_{};
    std::atomic<std::uint64_t> gateway_ticket_replay_total_{};
    std::atomic<std::uint64_t> gate_login_success_total_{};
    std::atomic<std::uint64_t> gate_login_failed_total_{};
    std::atomic<std::uint64_t> game_session_expired_total_{};
    std::atomic<std::uint64_t> reconnect_success_total_{};
    std::atomic<std::uint64_t> reconnect_failed_total_{};
    std::atomic<std::uint64_t> internal_auth_failed_total_{};
    mutable std::mutex upstream_mutex_;
    std::unordered_map<std::string, UpstreamInstanceMetrics> upstream_instances_;
};

}  // namespace mmo::runtime::observability

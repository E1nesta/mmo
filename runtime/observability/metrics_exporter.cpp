#include "runtime/observability/metrics_exporter.h"

#include <cstdint>
#include <sstream>
#include <string>

namespace runtime::observability {
namespace {

void append_metric(
    std::ostringstream& output,
    const std::string& name,
    const std::string& type,
    std::uint64_t value) {
    output << "# TYPE " << name << ' ' << type << '\n'
           << name << ' ' << value << '\n';
}

void append_metric_type(
    std::ostringstream& output,
    const std::string& name,
    const std::string& type) {
    output << "# TYPE " << name << ' ' << type << '\n';
}

void append_labeled_sample(
    std::ostringstream& output,
    const std::string& name,
    const std::string& labels,
    std::uint64_t value) {
    output << name << '{' << labels << "} " << value << '\n';
}

std::string upstream_labels(const UpstreamInstanceMetrics& metrics) {
    return "service=\"" + metrics.service + "\",instance_id=\"" +
           metrics.instance_id + "\"";
}

}  // namespace

std::string render_prometheus_metrics(const MetricsSnapshot& snapshot) {
    std::ostringstream output;

    append_metric(output, "mmo_active_connections", "gauge",
                  snapshot.active_connections);
    append_metric(output, "mmo_accepted_connections_total", "counter",
                  snapshot.accepted_connections);
    append_metric(output, "mmo_requests_total", "counter",
                  snapshot.requests_total);
    append_metric(output, "mmo_errors_total", "counter",
                  snapshot.errors_total);
    append_metric(output, "mmo_bytes_read_total", "counter",
                  snapshot.bytes_read_total);
    append_metric(output, "mmo_bytes_written_total", "counter",
                  snapshot.bytes_written_total);
    append_metric(output, "mmo_handler_latency_ms_total", "counter",
                  snapshot.handler_latency_ms_total);
    append_metric(output, "mmo_executor_queue_depth", "gauge",
                  snapshot.executor_queue_depth);
    append_metric(output, "mmo_executor_post_failed_total", "counter",
                  snapshot.executor_post_failed_total);
    append_metric(output, "mmo_executor_queue_overflow_total", "counter",
                  snapshot.executor_queue_overflow_total);
    append_metric(output, "mmo_handler_rejected_total", "counter",
                  snapshot.handler_rejected_total);
    append_metric(output, "mmo_rpc_pending_count", "gauge",
                  snapshot.rpc_pending_count);
    append_metric(output, "mmo_rpc_calls_total", "counter",
                  snapshot.rpc_calls_total);
    append_metric(output, "mmo_rpc_errors_total", "counter",
                  snapshot.rpc_errors_total);
    append_metric(output, "mmo_rpc_timeout_total", "counter",
                  snapshot.rpc_timeout_total);
    append_metric(output, "mmo_rpc_remote_error_total", "counter",
                  snapshot.rpc_remote_error_total);
    append_metric(output, "mmo_login_success_total", "counter",
                  snapshot.login_success_total);
    append_metric(output, "mmo_login_failed_total", "counter",
                  snapshot.login_failed_total);
    append_metric(output, "mmo_gateway_ticket_issued_total", "counter",
                  snapshot.gateway_ticket_issued_total);
    append_metric(output, "mmo_gateway_ticket_rejected_total", "counter",
                  snapshot.gateway_ticket_rejected_total);
    append_metric(output, "mmo_gateway_ticket_replay_total", "counter",
                  snapshot.gateway_ticket_replay_total);
    append_metric(output, "mmo_gateway_login_success_total", "counter",
                  snapshot.gateway_login_success_total);
    append_metric(output, "mmo_gateway_login_failed_total", "counter",
                  snapshot.gateway_login_failed_total);
    append_metric(output, "mmo_game_session_expired_total", "counter",
                  snapshot.game_session_expired_total);
    append_metric(output, "mmo_reconnect_success_total", "counter",
                  snapshot.reconnect_success_total);
    append_metric(output, "mmo_reconnect_failed_total", "counter",
                  snapshot.reconnect_failed_total);
    if (!snapshot.upstream_instances.empty()) {
        append_metric_type(output, "mmo_upstream_instance_healthy", "gauge");
        append_metric_type(output, "mmo_upstream_instance_pending", "gauge");
        append_metric_type(
            output,
            "mmo_upstream_instance_unhealthy_total",
            "counter");
        append_metric_type(
            output,
            "mmo_upstream_instance_recovered_total",
            "counter");
        append_metric_type(
            output,
            "mmo_upstream_instance_failover_total",
            "counter");
        append_metric_type(
            output,
            "mmo_upstream_instance_circuit_open_total",
            "counter");
        append_metric_type(
            output,
            "mmo_upstream_instance_request_timeout_total",
            "counter");
        append_metric_type(
            output,
            "mmo_upstream_instance_remote_error_total",
            "counter");
    }
    for (const auto& upstream : snapshot.upstream_instances) {
        const auto labels = upstream_labels(upstream);
        append_labeled_sample(
            output,
            "mmo_upstream_instance_healthy",
            labels,
            upstream.healthy);
        append_labeled_sample(
            output,
            "mmo_upstream_instance_pending",
            labels,
            upstream.pending);
        append_labeled_sample(
            output,
            "mmo_upstream_instance_unhealthy_total",
            labels,
            upstream.unhealthy_total);
        append_labeled_sample(
            output,
            "mmo_upstream_instance_recovered_total",
            labels,
            upstream.recovered_total);
        append_labeled_sample(
            output,
            "mmo_upstream_instance_failover_total",
            labels,
            upstream.failover_total);
        append_labeled_sample(
            output,
            "mmo_upstream_instance_circuit_open_total",
            labels,
            upstream.circuit_open_total);
        append_labeled_sample(
            output,
            "mmo_upstream_instance_request_timeout_total",
            labels,
            upstream.request_timeout_total);
        append_labeled_sample(
            output,
            "mmo_upstream_instance_remote_error_total",
            labels,
            upstream.remote_error_total);
    }

    return output.str();
}

}  // namespace runtime::observability

#include "runtime/observability/metrics_exporter.h"

#include <cstdint>
#include <sstream>
#include <string>

namespace mmo::runtime::observability {
namespace {

void append_metric(
    std::ostringstream& output,
    const std::string& name,
    const std::string& type,
    std::uint64_t value) {
    output << "# TYPE " << name << ' ' << type << '\n'
           << name << ' ' << value << '\n';
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
    append_metric(output, "mmo_channel_pending_count", "gauge",
                  snapshot.channel_pending_count);
    append_metric(output, "mmo_channel_requests_total", "counter",
                  snapshot.channel_requests_total);
    append_metric(output, "mmo_channel_errors_total", "counter",
                  snapshot.channel_errors_total);
    append_metric(output, "mmo_channel_pending_limit_total", "counter",
                  snapshot.channel_pending_limit_total);
    append_metric(output, "mmo_rpc_pending_count", "gauge",
                  snapshot.rpc_pending_count);
    append_metric(output, "mmo_rpc_requests_total", "counter",
                  snapshot.rpc_requests_total);
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
    append_metric(output, "mmo_gate_login_success_total", "counter",
                  snapshot.gate_login_success_total);
    append_metric(output, "mmo_gate_login_failed_total", "counter",
                  snapshot.gate_login_failed_total);
    append_metric(output, "mmo_game_session_expired_total", "counter",
                  snapshot.game_session_expired_total);
    append_metric(output, "mmo_reconnect_success_total", "counter",
                  snapshot.reconnect_success_total);
    append_metric(output, "mmo_reconnect_failed_total", "counter",
                  snapshot.reconnect_failed_total);
    append_metric(output, "mmo_internal_auth_failed_total", "counter",
                  snapshot.internal_auth_failed_total);

    return output.str();
}

}  // namespace mmo::runtime::observability

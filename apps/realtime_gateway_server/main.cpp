#include "apps/realtime_gateway_server/realtime_route_table.h"

#include <string>

#include "runtime/observability/logging.h"
#include "runtime/service/service_app.h"
#include "runtime/service/service_runtime.h"

int main() {
    runtime::service::ServiceApp app("realtime_gateway_server");
    const auto tcp_options = runtime::service::make_server_transport_options(app);
    auto rpc_client = runtime::service::make_rpc_client(app, tcp_options);
    const auto route_table =
        apps::realtime_gateway_server::make_realtime_route_table();

    runtime::observability::log_info(
        runtime::observability::LogContext{app.service_name()},
        "realtime_gateway_skeleton_ready udp_kcp_port=" +
            std::to_string(app.service_config().udp_kcp_port) +
            " routes=" + std::to_string(route_table.size()));
    (void)rpc_client;
    return 0;
}

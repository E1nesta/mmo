#include <exception>
#include <string>

#include "apps/api_gateway_server/api_handler.h"
#include "apps/api_gateway_server/api_http_server.h"
#include "runtime/service/service_app.h"
#include "runtime/observability/logging.h"
#include "runtime/observability/metrics.h"
#include "runtime/service/service_runtime.h"

int main() {
    try {
        runtime::service::ServiceApp app("api_gateway_server");
        const auto tcp_options =
            runtime::service::make_server_transport_options(app);
        auto rpc_client =
            runtime::service::make_rpc_client(app, tcp_options);
        runtime::observability::MetricsRegistry metrics;
        apps::api_gateway_server::ApiHandler handler(
            app.config(), *rpc_client, metrics);

        runtime::observability::log_info(
            runtime::observability::LogContext{app.service_name()},
            "http_server_listening port=" +
                std::to_string(app.service_config().tcp_port));

        return apps::api_gateway_server::run_http_server(
            app.service_config().tcp_port,
            handler,
            app.config().scheduler.io_threads);
    } catch (const std::exception& error) {
        runtime::observability::log_error(
            runtime::observability::LogContext{"api_gateway_server"},
            std::string("http_server_fatal error=") + error.what());
        return 1;
    }
}

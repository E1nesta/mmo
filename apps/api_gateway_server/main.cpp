#include <exception>
#include <string>

#include "apps/api_gateway_server/api_handler.h"
#include "apps/api_gateway_server/api_http_server.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/observability/metrics.h"
#include "runtime/routing/gateway_forwarder.h"
#include "runtime/transport/envelope_transport.h"

int main() {
    try {
        mmo::runtime::foundation::ServerApp app("api_gateway_server");
        const auto tcp_options =
            mmo::runtime::transport::make_transport_options(
                app.config().transport.tcp, app.config().execution);
        mmo::runtime::routing::GatewayForwarder forwarder(
            app.service_name(), app.config(), tcp_options);
        mmo::runtime::observability::MetricsRegistry metrics;
        mmo::apps::api_gateway_server::ApiHandler handler(
            app.config(), forwarder, metrics);

        mmo::runtime::observability::log_info(
            mmo::runtime::observability::LogContext{app.service_name()},
            "http_server_listening port=" +
                std::to_string(app.service_config().tcp_port));

        return mmo::apps::api_gateway_server::run_http_server(
            app.service_config().tcp_port, handler);
    } catch (const std::exception& error) {
        mmo::runtime::observability::log_error(
            mmo::runtime::observability::LogContext{"api_gateway_server"},
            std::string("http_server_fatal error=") + error.what());
        return 1;
    }
}

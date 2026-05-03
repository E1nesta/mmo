#include "runtime/rpc/rpc_server_app.h"

#include <memory>

#include "runtime/channel/channel_connection_pool.h"
#include "runtime/channel/service_registry.h"
#include "runtime/observability/logging.h"
#include "runtime/transport/tcp_envelope_server.h"

namespace mmo::runtime::rpc {

mmo::runtime::transport::TransportOptions make_server_transport_options(
    const mmo::runtime::foundation::ServerApp& app) {
    return mmo::runtime::transport::make_transport_options(
        app.config().transport.tcp, app.config().execution);
}

std::unique_ptr<RpcClient> make_static_rpc_client(
    const mmo::runtime::foundation::ServerApp& app,
    const mmo::runtime::transport::TransportOptions& transport_options) {
    auto service_registry =
        std::make_shared<mmo::runtime::channel::StaticServiceRegistry>(
            app.config());
    return std::make_unique<RpcClient>(
        service_registry,
        transport_options,
        mmo::runtime::channel::make_channel_connection_pool_options(
            app.config().channel),
        make_rpc_client_options(app.service_name(), app.config()));
}

int run_tcp_rpc_server(
    const mmo::runtime::foundation::ServerApp& app,
    const RpcServer& rpc_server,
    const mmo::runtime::transport::TransportOptions& transport_options) {
    mmo::runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        rpc_server.handler(),
        app.service_name(),
        transport_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

}  // namespace mmo::runtime::rpc

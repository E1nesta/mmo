#include "apps/instance_server/instance_handlers.h"
#include "modules/instance/instance_service.h"
#include "runtime/server/server_app.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/server/server_bootstrap.h"

int main() {
    mmo::runtime::server::ServerApp app("instance_server");
    const auto tcp_options = mmo::runtime::server::make_server_transport_options(app);
    auto rpc_client = mmo::runtime::server::make_static_rpc_client(app, tcp_options);

    mmo::modules::instance::InstanceService service;
    mmo::runtime::rpc::RpcServer rpc_server(
        mmo::runtime::rpc::make_rpc_server_options(app.config()));
    mmo::apps::instance_server::register_instance_handlers(
        rpc_server, service, *rpc_client, app.service_name());

    return mmo::runtime::server::run_tcp_rpc_server(app, rpc_server, tcp_options);
}

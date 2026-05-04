#include "apps/world_server/world_handlers.h"
#include "modules/world/world_service.h"
#include "runtime/server/server_app.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/server/server_bootstrap.h"

int main() {
    runtime::server::ServerApp app("world_server");
    const auto tcp_options = runtime::server::make_server_transport_options(app);
    auto rpc_client = runtime::server::make_static_rpc_client(app, tcp_options);

    modules::world::WorldService service;
    runtime::rpc::RpcServer rpc_server(
        runtime::rpc::make_rpc_server_options(app.config()));
    apps::world_server::register_world_handlers(
        rpc_server, service, *rpc_client, app.service_name());

    return runtime::server::run_tcp_rpc_server(app, rpc_server, tcp_options);
}

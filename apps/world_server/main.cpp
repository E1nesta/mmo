#include "apps/world_server/world_handlers.h"
#include "modules/world/world_service.h"
#include "runtime/foundation/server_app.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/rpc/rpc_server_app.h"

int main() {
    mmo::runtime::foundation::ServerApp app("world_server");
    const auto tcp_options = mmo::runtime::rpc::make_server_transport_options(app);
    auto rpc_client = mmo::runtime::rpc::make_static_rpc_client(app, tcp_options);

    mmo::modules::world::WorldService service;
    mmo::runtime::rpc::RpcServer rpc_server(
        mmo::runtime::rpc::make_rpc_server_options(app.config()));
    mmo::apps::world_server::register_world_handlers(
        rpc_server, service, *rpc_client, app.service_name());

    return mmo::runtime::rpc::run_tcp_rpc_server(app, rpc_server, tcp_options);
}

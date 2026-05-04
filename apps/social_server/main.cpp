#include "apps/social_server/social_handlers.h"
#include "modules/social/social_boundary.h"
#include "runtime/server/server_app.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/server/server_bootstrap.h"

int main() {
    runtime::server::ServerApp app("social_server");
    const auto tcp_options = runtime::server::make_server_transport_options(app);

    modules::social::SocialBoundaryService service;
    runtime::rpc::RpcServer rpc_server(
        runtime::rpc::make_rpc_server_options(app.config()));
    apps::social_server::register_social_handlers(rpc_server, service);

    return runtime::server::run_tcp_rpc_server(app, rpc_server, tcp_options);
}

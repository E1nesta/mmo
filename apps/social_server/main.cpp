#include "apps/social_server/social_handlers.h"
#include "modules/social/social_boundary.h"
#include "runtime/foundation/server_app.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/rpc/rpc_server_app.h"

int main() {
    mmo::runtime::foundation::ServerApp app("social_server");
    const auto tcp_options = mmo::runtime::rpc::make_server_transport_options(app);

    mmo::modules::social::SocialBoundaryService service;
    mmo::runtime::rpc::RpcServer rpc_server(
        mmo::runtime::rpc::make_rpc_server_options(app.config()));
    mmo::apps::social_server::register_social_handlers(rpc_server, service);

    return mmo::runtime::rpc::run_tcp_rpc_server(app, rpc_server, tcp_options);
}

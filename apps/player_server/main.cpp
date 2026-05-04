#include <memory>

#include "apps/player_server/player_handlers.h"
#include "modules/player/mysql_player_repository.h"
#include "modules/player/player_service.h"
#include "runtime/server/server_app.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/server/server_bootstrap.h"

int main() {
    runtime::server::ServerApp app("player_server");
    const auto tcp_options = runtime::server::make_server_transport_options(app);

    auto mysql_pool = runtime::server::require_mysql_pool(app);
    if (!mysql_pool) {
        return 1;
    }
    auto player_repository =
        std::make_shared<modules::player::MysqlPlayerRepository>(mysql_pool);
    modules::player::PlayerService service(player_repository);
    runtime::rpc::RpcServer rpc_server(
        runtime::rpc::make_rpc_server_options(app.config()));
    apps::player_server::register_player_handlers(rpc_server, service);

    return runtime::server::run_tcp_rpc_server(app, rpc_server, tcp_options);
}

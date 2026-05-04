#include <memory>

#include "apps/player_server/player_handlers.h"
#include "modules/player/mysql_player_repository.h"
#include "modules/player/player_service.h"
#include "runtime/server/server_app.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/server/server_bootstrap.h"

int main() {
    mmo::runtime::server::ServerApp app("player_server");
    const auto tcp_options = mmo::runtime::server::make_server_transport_options(app);

    auto mysql_pool = mmo::runtime::server::require_mysql_pool(app);
    if (!mysql_pool) {
        return 1;
    }
    auto player_repository =
        std::make_shared<mmo::modules::player::MysqlPlayerRepository>(mysql_pool);
    mmo::modules::player::PlayerService service(player_repository);
    mmo::runtime::rpc::RpcServer rpc_server(
        mmo::runtime::rpc::make_rpc_server_options(app.config()));
    mmo::apps::player_server::register_player_handlers(rpc_server, service);

    return mmo::runtime::server::run_tcp_rpc_server(app, rpc_server, tcp_options);
}

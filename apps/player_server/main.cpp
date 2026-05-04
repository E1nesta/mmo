#include <memory>
#include <string>

#include "apps/player_server/player_handlers.h"
#include "modules/player/mysql_player_repository.h"
#include "modules/player/player_service.h"
#include "runtime/server/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/server/server_bootstrap.h"
#include "runtime/storage/storage_bootstrap.h"

int main() {
    mmo::runtime::server::ServerApp app("player_server");
    const auto tcp_options = mmo::runtime::server::make_server_transport_options(app);

    std::shared_ptr<mmo::runtime::storage::MysqlConnectionPool> mysql_pool;
    std::string storage_error;
    if (!mmo::runtime::storage::initialize_mysql_pool(
            app.config(), &mysql_pool, &storage_error)) {
        mmo::runtime::observability::log_error(
            mmo::runtime::observability::LogContext{app.service_name()},
            "mysql_pool_init_failed error=" + storage_error);
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

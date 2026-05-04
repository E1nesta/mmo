#include <memory>

#include "apps/auth_server/auth_handlers.h"
#include "modules/auth/auth_service.h"
#include "modules/auth/mysql_account_repository.h"
#include "modules/auth/mysql_player_identity_repository.h"
#include "runtime/server/server_app.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/server/server_bootstrap.h"

int main() {
    mmo::runtime::server::ServerApp app("auth_server");
    const auto tcp_options = mmo::runtime::server::make_server_transport_options(app);

    auto mysql_pool = mmo::runtime::server::require_mysql_pool(app);
    if (!mysql_pool) {
        return 1;
    }

    auto account_repository =
        std::make_shared<mmo::modules::auth::MysqlAccountRepository>(mysql_pool);
    auto identity_repository =
        std::make_shared<mmo::modules::auth::MysqlPlayerIdentityRepository>(
            mysql_pool);
    mmo::modules::auth::AuthService service(
        account_repository, identity_repository);
    mmo::runtime::rpc::RpcServer rpc_server(
        mmo::runtime::rpc::make_rpc_server_options(app.config()));
    mmo::apps::auth_server::register_auth_handlers(
        rpc_server, service, app.config(), app.service_name());

    return mmo::runtime::server::run_tcp_rpc_server(app, rpc_server, tcp_options);
}

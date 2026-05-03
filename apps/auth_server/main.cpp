#include <memory>
#include <string>

#include "apps/auth_server/auth_handlers.h"
#include "modules/auth/auth_service.h"
#include "modules/auth/mysql_account_repository.h"
#include "modules/auth/mysql_player_identity_repository.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/rpc/rpc_server_app.h"
#include "runtime/storage/storage_bootstrap.h"

int main() {
    mmo::runtime::foundation::ServerApp app("auth_server");
    const auto tcp_options = mmo::runtime::rpc::make_server_transport_options(app);

    std::shared_ptr<mmo::runtime::storage::MysqlConnectionPool> mysql_pool;
    std::string storage_error;
    if (!mmo::runtime::storage::initialize_mysql_pool(
            app.config(), &mysql_pool, &storage_error)) {
        mmo::runtime::observability::log_error(
            mmo::runtime::observability::LogContext{app.service_name()},
            "mysql_pool_init_failed error=" + storage_error);
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

    return mmo::runtime::rpc::run_tcp_rpc_server(app, rpc_server, tcp_options);
}

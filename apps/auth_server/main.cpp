#include <memory>

#include "apps/auth_server/auth_handlers.h"
#include "modules/auth/auth_service.h"
#include "modules/auth/mysql_account_repository.h"
#include "modules/auth/mysql_player_identity_repository.h"
#include "runtime/service/service_app.h"
#include "runtime/service/service_runtime.h"

int main() {
    runtime::service::ServiceApp app("auth_server");
    const auto tcp_options = runtime::service::make_server_transport_options(app);

    auto mysql_pool = runtime::service::require_mysql_pool(app);
    if (!mysql_pool) {
        return 1;
    }

    auto account_repository =
        std::make_shared<modules::auth::MysqlAccountRepository>(mysql_pool);
    auto identity_repository =
        std::make_shared<modules::auth::MysqlPlayerIdentityRepository>(
            mysql_pool);
    modules::auth::AuthService service(
        account_repository, identity_repository);
    runtime::rpc::RpcDispatcher dispatcher;
    apps::auth_server::register_auth_handlers(
        dispatcher, service, app.config(), app.service_name());

    return runtime::service::run_rpc_service(app, dispatcher, tcp_options);
}

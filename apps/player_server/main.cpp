#include <memory>

#include "apps/player_server/player_handlers.h"
#include "modules/player/mysql_player_repository.h"
#include "modules/player/player_service.h"
#include "runtime/entity/entity_executor.h"
#include "runtime/entity/entity_router.h"
#include "runtime/scheduler/sharded_executor.h"
#include "runtime/service/service_app.h"
#include "runtime/service/service_runtime.h"

int main() {
    runtime::service::ServiceApp app("player_server");
    const auto tcp_options = runtime::service::make_server_transport_options(app);

    auto mysql_pool = runtime::service::require_mysql_pool(app);
    if (!mysql_pool) {
        return 1;
    }
    auto player_repository =
        std::make_shared<modules::player::MysqlPlayerRepository>(mysql_pool);
    modules::player::PlayerService service(player_repository);
    runtime::rpc::RpcDispatcher dispatcher;
    runtime::entity::EntityRouter entity_router;
    runtime::entity::EntityExecutor entity_executor(entity_router);
    runtime::scheduler::ShardedExecutor entity_scheduler(
        runtime::service::make_entity_scheduler_options(app));
    apps::player_server::register_player_handlers(
        dispatcher, service, entity_executor, entity_scheduler);

    return runtime::service::run_rpc_service(app, dispatcher, tcp_options);
}

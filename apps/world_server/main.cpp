#include "apps/world_server/world_handlers.h"
#include "modules/world/world_service.h"
#include "runtime/entity/entity_executor.h"
#include "runtime/entity/entity_router.h"
#include "runtime/scheduler/sharded_executor.h"
#include "runtime/service/service_app.h"
#include "runtime/service/service_runtime.h"

int main() {
    runtime::service::ServiceApp app("world_server");
    const auto tcp_options = runtime::service::make_server_transport_options(app);
    auto rpc_client = runtime::service::make_rpc_client(app, tcp_options);
    runtime::rpc::RpcDispatcher dispatcher;
    runtime::entity::EntityRouter entity_router;
    runtime::entity::EntityExecutor entity_executor(entity_router);
    runtime::scheduler::ShardedExecutor entity_scheduler(
        runtime::service::make_entity_scheduler_options(app));

    modules::world::WorldService service;
    apps::world_server::register_world_handlers(
        dispatcher,
        service,
        *rpc_client,
        entity_executor,
        entity_scheduler,
        app.service_name());

    return runtime::service::run_rpc_service(app, dispatcher, tcp_options);
}

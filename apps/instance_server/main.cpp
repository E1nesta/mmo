#include "apps/instance_server/instance_handlers.h"
#include "modules/instance/instance_service.h"
#include "runtime/entity/entity_executor.h"
#include "runtime/entity/entity_router.h"
#include "runtime/scheduler/sharded_executor.h"
#include "runtime/service/service_app.h"
#include "runtime/service/service_runtime.h"

int main() {
    runtime::service::ServiceApp app("instance_server");
    const auto tcp_options = runtime::service::make_server_transport_options(app);
    auto rpc_client = runtime::service::make_rpc_client(app, tcp_options);
    runtime::rpc::RpcDispatcher dispatcher;

    modules::instance::InstanceService service;
    runtime::entity::EntityRouter entity_router;
    runtime::entity::EntityExecutor entity_executor(entity_router);
    runtime::scheduler::ShardedExecutor entity_scheduler(
        runtime::service::make_entity_scheduler_options(app));
    apps::instance_server::register_instance_handlers(
        dispatcher,
        service,
        entity_executor,
        entity_scheduler,
        *rpc_client,
        app.service_name());

    return runtime::service::run_rpc_service(app, dispatcher, tcp_options);
}

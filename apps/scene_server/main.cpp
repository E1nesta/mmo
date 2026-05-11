#include "apps/scene_server/scene_handlers.h"
#include "modules/scene/scene_service.h"
#include "runtime/entity/entity_executor.h"
#include "runtime/entity/entity_router.h"
#include "runtime/scheduler/sharded_executor.h"
#include "runtime/service/service_app.h"
#include "runtime/service/service_runtime.h"

int main() {
    runtime::service::ServiceApp app("scene_server");
    const auto tcp_options = runtime::service::make_server_transport_options(app);

    modules::scene::SceneService service;
    runtime::rpc::RpcDispatcher dispatcher;
    runtime::entity::EntityRouter entity_router;
    runtime::entity::EntityExecutor entity_executor(entity_router);
    runtime::scheduler::ShardedExecutor entity_scheduler(
        runtime::service::make_entity_scheduler_options(app));
    apps::scene_server::register_scene_handlers(
        dispatcher, service, entity_executor, entity_scheduler);

    return runtime::service::run_rpc_service(app, dispatcher, tcp_options);
}

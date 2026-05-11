#include "apps/social_server/social_handlers.h"
#include "modules/social/social_boundary.h"
#include "runtime/entity/entity_executor.h"
#include "runtime/entity/entity_router.h"
#include "runtime/scheduler/sharded_executor.h"
#include "runtime/service/service_app.h"
#include "runtime/service/service_runtime.h"

int main() {
    runtime::service::ServiceApp app("social_server");
    const auto tcp_options = runtime::service::make_server_transport_options(app);

    modules::social::SocialBoundaryService service;
    runtime::rpc::RpcDispatcher dispatcher;
    runtime::entity::EntityRouter entity_router;
    runtime::entity::EntityExecutor entity_executor(entity_router);
    runtime::scheduler::ShardedExecutor entity_scheduler(
        runtime::service::make_entity_scheduler_options(app));
    apps::social_server::register_social_handlers(
        dispatcher, service, entity_executor, entity_scheduler);

    return runtime::service::run_rpc_service(app, dispatcher, tcp_options);
}

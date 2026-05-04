#pragma once

#include <string>

#include "modules/auth/auth_service.h"
#include "runtime/foundation/server_config.h"
#include "runtime/rpc/rpc_server.h"

namespace apps::auth_server {

void register_auth_handlers(
    runtime::rpc::RpcServer& rpc_server,
    modules::auth::AuthService& service,
    const runtime::foundation::ServerConfig& config,
    const std::string& service_name);

}  // namespace apps::auth_server

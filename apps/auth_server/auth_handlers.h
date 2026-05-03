#pragma once

#include <string>

#include "modules/auth/auth_service.h"
#include "runtime/foundation/server_config.h"
#include "runtime/rpc/rpc_server.h"

namespace mmo::apps::auth_server {

void register_auth_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::auth::AuthService& service,
    const mmo::runtime::foundation::ServerConfig& config,
    const std::string& service_name);

}  // namespace mmo::apps::auth_server

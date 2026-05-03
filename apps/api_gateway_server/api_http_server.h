#pragma once

#include "apps/api_gateway_server/api_handler.h"

namespace mmo::apps::api_gateway_server {

int run_http_server(int port, ApiHandler& handler);

}  // namespace mmo::apps::api_gateway_server

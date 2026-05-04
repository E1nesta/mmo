#pragma once

#include "apps/api_gateway_server/api_handler.h"

namespace apps::api_gateway_server {

int run_http_server(int port, ApiHandler& handler, int thread_count);

}  // namespace apps::api_gateway_server

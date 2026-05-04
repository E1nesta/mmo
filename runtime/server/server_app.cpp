#include "runtime/server/server_app.h"

#include <utility>

namespace runtime::server {

ServerApp::ServerApp(std::string service_name)
    : service_name_(std::move(service_name)),
      config_(runtime::foundation::load_server_config_from_env()) {}

const std::string& ServerApp::service_name() const {
    return service_name_;
}

const runtime::foundation::ServerConfig& ServerApp::config() const {
    return config_;
}

const runtime::foundation::ServiceConfig& ServerApp::service_config() const {
    return config_.service(service_name_);
}

const runtime::foundation::ServiceConfig& ServerApp::service_config(
    const std::string& service_name) const {
    return config_.service(service_name);
}

}  // namespace runtime::server

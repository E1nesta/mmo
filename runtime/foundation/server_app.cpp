#include "runtime/foundation/server_app.h"

#include <utility>

namespace mmo::runtime::foundation {

ServerApp::ServerApp(std::string service_name)
    : service_name_(std::move(service_name)),
      config_(load_server_config_from_env()) {}

const std::string& ServerApp::service_name() const {
    return service_name_;
}

const ServerConfig& ServerApp::config() const {
    return config_;
}

const ServiceConfig& ServerApp::service_config() const {
    return config_.service(service_name_);
}

const ServiceConfig& ServerApp::service_config(const std::string& service_name) const {
    return config_.service(service_name);
}

}  // namespace mmo::runtime::foundation

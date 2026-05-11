#include "runtime/service/service_app.h"

#include <utility>

namespace runtime::service {

ServiceApp::ServiceApp(std::string service_name)
    : service_name_(std::move(service_name)),
      config_(runtime::foundation::load_server_config_from_env()) {}

const std::string& ServiceApp::service_name() const {
    return service_name_;
}

const runtime::foundation::ServerConfig& ServiceApp::config() const {
    return config_;
}

const runtime::foundation::ServiceConfig& ServiceApp::service_config() const {
    return config_.service(service_name_);
}

const runtime::foundation::ServiceConfig& ServiceApp::service_config(
    const std::string& service_name) const {
    return config_.service(service_name);
}

}  // namespace runtime::service

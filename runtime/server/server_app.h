#pragma once

#include <string>

#include "runtime/foundation/server_config.h"

namespace mmo::runtime::server {

class ServerApp {
public:
    explicit ServerApp(std::string service_name);

    const std::string& service_name() const;
    const mmo::runtime::foundation::ServerConfig& config() const;
    const mmo::runtime::foundation::ServiceConfig& service_config() const;
    const mmo::runtime::foundation::ServiceConfig& service_config(
        const std::string& service_name) const;

private:
    std::string service_name_;
    mmo::runtime::foundation::ServerConfig config_;
};

}  // namespace mmo::runtime::server

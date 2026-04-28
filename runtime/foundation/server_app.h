#pragma once

#include <string>

#include "runtime/foundation/server_config.h"

namespace mmo::runtime::foundation {

class ServerApp {
public:
    explicit ServerApp(std::string service_name);

    const std::string& service_name() const;
    const ServerConfig& config() const;
    const ServiceConfig& service_config() const;
    const ServiceConfig& service_config(const std::string& service_name) const;

private:
    std::string service_name_;
    ServerConfig config_;
};

}  // namespace mmo::runtime::foundation

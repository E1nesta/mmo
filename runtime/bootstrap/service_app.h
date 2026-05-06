#pragma once

#include <string>

#include "runtime/foundation/server_config.h"

namespace runtime::bootstrap {

class ServiceApp {
public:
    explicit ServiceApp(std::string service_name);

    const std::string& service_name() const;
    const runtime::foundation::ServerConfig& config() const;
    const runtime::foundation::ServiceConfig& service_config() const;
    const runtime::foundation::ServiceConfig& service_config(
        const std::string& service_name) const;

private:
    std::string service_name_;
    runtime::foundation::ServerConfig config_;
};

}  // namespace runtime::bootstrap

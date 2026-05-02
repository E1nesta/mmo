#include "runtime/channel/static_endpoint_resolver.h"

#include <stdexcept>
#include <utility>

namespace mmo::runtime::channel {

StaticEndpointResolver::StaticEndpointResolver(
    const mmo::runtime::foundation::ServerConfig& config) {
    for (const auto& [service_name, service_config] : config.services) {
        if (service_config.instances.empty()) {
            throw std::runtime_error(
                "service instances must be configured: " + service_name);
        }
        const auto& instance = service_config.instances.front();
        mmo::runtime::transport::TransportEndpoint endpoint;
        endpoint.host = instance.host;
        endpoint.port = instance.tcp_port;
        endpoints_.emplace(service_name, std::move(endpoint));
    }
}

std::optional<mmo::runtime::transport::TransportEndpoint>
StaticEndpointResolver::resolve(const std::string& service_name) const {
    const auto it = endpoints_.find(service_name);
    if (it == endpoints_.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace mmo::runtime::channel

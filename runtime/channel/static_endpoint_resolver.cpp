#include "runtime/channel/static_endpoint_resolver.h"

namespace mmo::runtime::channel {

StaticEndpointResolver::StaticEndpointResolver(
    const mmo::runtime::foundation::ServerConfig& config) {
    for (const auto& [service_name, service_config] : config.services) {
        endpoints_.emplace(
            service_name,
            mmo::runtime::transport::make_transport_endpoint(service_config));
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

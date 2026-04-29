#pragma once

#include <string>
#include <unordered_map>

#include "runtime/channel/endpoint_resolver.h"
#include "runtime/foundation/server_config.h"

namespace mmo::runtime::channel {

class StaticEndpointResolver : public EndpointResolver {
public:
    explicit StaticEndpointResolver(
        const mmo::runtime::foundation::ServerConfig& config);

    std::optional<mmo::runtime::transport::TransportEndpoint> resolve(
        const std::string& service_name) const override;

private:
    std::unordered_map<
        std::string,
        mmo::runtime::transport::TransportEndpoint> endpoints_;
};

}  // namespace mmo::runtime::channel

#pragma once

#include <optional>
#include <string>

#include "runtime/transport/envelope_transport.h"

namespace runtime::channel {

class EndpointResolver {
public:
    virtual ~EndpointResolver() = default;

    virtual std::optional<runtime::transport::TransportEndpoint> resolve(
        const std::string& service_name) const = 0;
};

}  // namespace runtime::channel

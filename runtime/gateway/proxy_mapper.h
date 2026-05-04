#pragma once

#include <functional>
#include <string>

namespace mmo::runtime::gateway {

template <
    typename PublicRequest,
    typename InternalRequest,
    typename InternalResponse,
    typename PublicResponse>
struct ProxyMapper {
    using RequestMapper = std::function<InternalRequest(const PublicRequest&)>;
    using ResponseMapper =
        std::function<PublicResponse(const PublicRequest&, const InternalResponse&)>;
    using RouteKeyMapper = std::function<std::string(const PublicRequest&)>;

    std::string public_response_message_type;
    std::string invalid_public_request_message{"invalid public request"};
    std::string invalid_internal_response_message{"invalid upstream response"};
    RequestMapper map_request;
    ResponseMapper map_response;
    RouteKeyMapper route_key;
};

}  // namespace mmo::runtime::gateway

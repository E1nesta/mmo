#pragma once

#include <atomic>
#include <cstdint>

#include <boost/beast/http.hpp>

#include "runtime/foundation/server_config.h"
#include "runtime/observability/metrics.h"
#include "runtime/gateway/gateway_forwarder.h"

namespace mmo::apps::api_gateway_server {

class ApiHandler {
public:
    ApiHandler(
        const mmo::runtime::foundation::ServerConfig& config,
        mmo::runtime::gateway::GatewayForwarder& forwarder,
        mmo::runtime::observability::MetricsRegistry& metrics);

    boost::beast::http::response<boost::beast::http::string_body> handle(
        const boost::beast::http::request<boost::beast::http::string_body>&
            request);

private:
    boost::beast::http::response<boost::beast::http::string_body> handle_ready(
        const boost::beast::http::request<boost::beast::http::string_body>&
            request) const;
    boost::beast::http::response<boost::beast::http::string_body> handle_metrics(
        const boost::beast::http::request<boost::beast::http::string_body>&
            request) const;
    boost::beast::http::response<boost::beast::http::string_body> handle_servers(
        const boost::beast::http::request<boost::beast::http::string_body>&
            request) const;
    boost::beast::http::response<boost::beast::http::string_body> handle_login(
        const boost::beast::http::request<boost::beast::http::string_body>&
            request);

    const mmo::runtime::foundation::ServerConfig& config_;
    mmo::runtime::gateway::GatewayForwarder& forwarder_;
    mmo::runtime::observability::MetricsRegistry& metrics_;
    std::atomic<std::uint64_t> next_request_id_{1};
};

}  // namespace mmo::apps::api_gateway_server

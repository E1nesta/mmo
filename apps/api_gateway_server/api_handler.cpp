#include "apps/api_gateway_server/api_handler.h"

#include <chrono>
#include <cstddef>
#include <sstream>
#include <string>
#include <unordered_map>

#include "common/context.pb.h"
#include "internal/gateway_auth.pb.h"
#include "runtime/foundation/readiness.h"
#include "runtime/observability/metrics_exporter.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"

namespace mmo::apps::api_gateway_server {

namespace http = boost::beast::http;

namespace {

std::string json_escape(const std::string& value) {
    std::string output;
    output.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\':
                output += "\\\\";
                break;
            case '"':
                output += "\\\"";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                output.push_back(ch);
                break;
        }
    }
    return output;
}

int from_hex(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

std::string url_decode(const std::string& value) {
    std::string output;
    output.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+') {
            output.push_back(' ');
        } else if (value[index] == '%' && index + 2U < value.size()) {
            const int high = from_hex(value[index + 1U]);
            const int low = from_hex(value[index + 2U]);
            if (high >= 0 && low >= 0) {
                output.push_back(static_cast<char>((high << 4) | low));
                index += 2U;
            } else {
                output.push_back(value[index]);
            }
        } else {
            output.push_back(value[index]);
        }
    }
    return output;
}

std::unordered_map<std::string, std::string> parse_form_body(
    const std::string& body) {
    std::unordered_map<std::string, std::string> fields;
    std::size_t offset = 0;
    while (offset <= body.size()) {
        const std::size_t ampersand = body.find('&', offset);
        const std::size_t end =
            ampersand == std::string::npos ? body.size() : ampersand;
        const std::size_t equals = body.find('=', offset);
        if (equals != std::string::npos && equals < end) {
            fields[url_decode(body.substr(offset, equals - offset))] =
                url_decode(body.substr(equals + 1U, end - equals - 1U));
        }
        if (ampersand == std::string::npos) {
            break;
        }
        offset = ampersand + 1U;
    }
    return fields;
}

http::response<http::string_body> json_response(
    http::status status,
    const std::string& body,
    unsigned int version,
    bool keep_alive) {
    http::response<http::string_body> response{status, version};
    response.set(http::field::server, "mmo-api-gateway");
    response.set(http::field::content_type, "application/json");
    response.keep_alive(keep_alive);
    response.body() = body;
    response.prepare_payload();
    return response;
}

http::response<http::string_body> text_response(
    http::status status,
    const std::string& body,
    const std::string& content_type,
    unsigned int version,
    bool keep_alive) {
    http::response<http::string_body> response{status, version};
    response.set(http::field::server, "mmo-api-gateway");
    response.set(http::field::content_type, content_type);
    response.keep_alive(keep_alive);
    response.body() = body;
    response.prepare_payload();
    return response;
}

std::string error_body(int code, const std::string& message) {
    return "{\"success\":false,\"error_code\":" + std::to_string(code) +
           ",\"error_message\":\"" + json_escape(message) + "\"}";
}

std::string error_body(const std::string& code, const std::string& message) {
    return "{\"success\":false,\"error_code\":\"" + json_escape(code) +
           "\",\"error_message\":\"" + json_escape(message) + "\"}";
}

mmo::common::RequestContext make_context(std::uint64_t request_id) {
    mmo::common::RequestContext context;
    context.set_request_id(request_id);
    context.set_trace_id("api-gateway-" + std::to_string(request_id));
    return context;
}

std::string find_field(
    const std::unordered_map<std::string, std::string>& fields,
    const std::string& key) {
    const auto it = fields.find(key);
    return it == fields.end() ? std::string{} : it->second;
}

}  // namespace

ApiHandler::ApiHandler(
    const mmo::runtime::foundation::ServerConfig& config,
    mmo::runtime::gateway::GatewayForwarder& forwarder,
    mmo::runtime::observability::MetricsRegistry& metrics)
    : config_(config), forwarder_(forwarder), metrics_(metrics) {}

http::response<http::string_body> ApiHandler::handle(
    const http::request<http::string_body>& request) {
    const std::string target(request.target());
    if (request.method() == http::verb::get && target == "/health") {
        return json_response(
            http::status::ok,
            "{\"success\":true}",
            request.version(),
            request.keep_alive());
    }
    if (request.method() == http::verb::get && target == "/ready") {
        return handle_ready(request);
    }
    if (request.method() == http::verb::get && target == "/metrics") {
        return handle_metrics(request);
    }
    if (request.method() == http::verb::get && target == "/v1/servers") {
        return handle_servers(request);
    }
    if (request.method() == http::verb::post && target == "/v1/login") {
        return handle_login(request);
    }
    return json_response(
        http::status::not_found,
        error_body(404, "api route is not configured"),
        request.version(),
        request.keep_alive());
}

http::response<http::string_body> ApiHandler::handle_ready(
    const http::request<http::string_body>& request) const {
    const auto readiness =
        mmo::runtime::foundation::check_tcp_dependency(
            "auth_server",
            config_.service("auth_server"),
            std::chrono::milliseconds(300));
    if (!readiness.ready) {
        return json_response(
            http::status::service_unavailable,
            error_body(readiness.error_code, readiness.message),
            request.version(),
            request.keep_alive());
    }
    return json_response(
        http::status::ok,
        "{\"success\":true,\"ready\":true}",
        request.version(),
        request.keep_alive());
}

http::response<http::string_body> ApiHandler::handle_metrics(
    const http::request<http::string_body>& request) const {
    return text_response(
        http::status::ok,
        mmo::runtime::observability::render_prometheus_metrics(
            metrics_.snapshot()),
        "text/plain; version=0.0.4",
        request.version(),
        request.keep_alive());
}

http::response<http::string_body> ApiHandler::handle_servers(
    const http::request<http::string_body>& request) const {
    const auto& game = config_.service("game_gateway_server");
    const auto& realtime = config_.service("realtime_gateway_server");
    std::ostringstream body;
    body << "{\"success\":true"
         << ",\"game_gateway\":{\"host\":\"" << json_escape(game.host)
         << "\",\"tcp_port\":" << game.tcp_port << "}"
         << ",\"realtime_gateway\":{\"host\":\"" << json_escape(realtime.host)
         << "\",\"udp_kcp_port\":" << realtime.udp_kcp_port
         << ",\"enabled\":false,\"status\":\"reserved\"}"
         << "}";
    return json_response(
        http::status::ok, body.str(), request.version(), request.keep_alive());
}

http::response<http::string_body> ApiHandler::handle_login(
    const http::request<http::string_body>& request) {
    const auto fields = parse_form_body(request.body());
    const auto account_name = find_field(fields, "account_name");
    const auto password = find_field(fields, "password");
    const auto device_id = find_field(fields, "device_id");
    if (account_name.empty() || device_id.empty()) {
        metrics_.record_login_failed();
        return json_response(
            http::status::bad_request,
            error_body(400, "account_name and device_id are required"),
            request.version(),
            request.keep_alive());
    }

    const auto request_id = next_request_id_.fetch_add(1U);
    auto context = make_context(request_id);
    mmo::internal_api::GatewayAuthLoginRequest internal_request;
    *internal_request.mutable_context() = context;
    internal_request.set_account_name(account_name);
    internal_request.set_password(password);
    internal_request.set_device_id(device_id);

    const auto forward_result = forwarder_.forward(
        "auth_server",
        mmo::runtime::protocol::kGatewayAuthLoginRequest,
        context,
        internal_request);
    if (!forward_result.ok()) {
        if (forward_result.has_response() &&
            forward_result.response().message_type() ==
                mmo::runtime::protocol::kErrorResponse) {
            mmo::common::ResponseContext error_context;
            if (mmo::runtime::protocol::unpack_message(
                    forward_result.response(), error_context)) {
                metrics_.record_login_failed();
                const auto status =
                    error_context.error_code() == 401
                        ? http::status::unauthorized
                        : http::status::bad_gateway;
                return json_response(
                    status,
                    error_body(
                        error_context.error_code(),
                        error_context.error_message()),
                    request.version(),
                    request.keep_alive());
            }
        }
        metrics_.record_login_failed();
        return json_response(
            http::status::bad_gateway,
            error_body(502, "auth backend request failed"),
            request.version(),
            request.keep_alive());
    }

    mmo::internal_api::GatewayAuthLoginResponse response;
    if (!mmo::runtime::protocol::unpack_message(
            forward_result.response(), response)) {
        metrics_.record_login_failed();
        return json_response(
            http::status::bad_gateway,
            error_body(502, "invalid auth backend response"),
            request.version(),
            request.keep_alive());
    }
    if (!response.context().success()) {
        metrics_.record_login_failed();
        return json_response(
            http::status::unauthorized,
            error_body(
                response.context().error_code(),
                response.context().error_message()),
            request.version(),
            request.keep_alive());
    }

    metrics_.record_login_success();
    metrics_.record_gateway_ticket_issued();

    std::ostringstream body;
    body << "{\"success\":true"
         << ",\"account_id\":" << response.account_id()
         << ",\"player_id\":" << response.player_id()
         << ",\"session_token\":\"" << json_escape(response.session_token())
         << "\",\"access_token\":\"" << json_escape(response.access_token())
         << "\",\"gateway_ticket\":\"" << json_escape(response.gateway_ticket())
         << "\",\"access_token_expires_at_epoch_millis\":"
         << response.access_token_expires_at_epoch_millis()
         << ",\"gateway_ticket_expires_at_epoch_millis\":"
         << response.gateway_ticket_expires_at_epoch_millis()
         << "}";
    return json_response(
        http::status::ok, body.str(), request.version(), request.keep_alive());
}

}  // namespace mmo::apps::api_gateway_server

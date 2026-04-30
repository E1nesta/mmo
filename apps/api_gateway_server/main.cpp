#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include "internal/gateway_auth.pb.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/observability/metrics.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/routing/gateway_forwarder.h"
#include "runtime/transport/envelope_transport.h"

namespace {

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

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

std::string error_body(int code, const std::string& message) {
    return "{\"success\":false,\"error_code\":" + std::to_string(code) +
           ",\"error_message\":\"" + json_escape(message) + "\"}";
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

class ApiHandler {
public:
    ApiHandler(
        const mmo::runtime::foundation::ServerConfig& config,
        mmo::runtime::routing::GatewayForwarder& forwarder,
        mmo::runtime::observability::MetricsRegistry& metrics)
        : config_(config), forwarder_(forwarder), metrics_(metrics) {}

    http::response<http::string_body> handle(
        const http::request<http::string_body>& request) {
        const std::string target(request.target());
        if (request.method() == http::verb::get && target == "/health") {
            return json_response(
                http::status::ok,
                "{\"success\":true}",
                request.version(),
                request.keep_alive());
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

private:
    http::response<http::string_body> handle_servers(
        const http::request<http::string_body>& request) const {
        const auto& game = config_.service("game_gateway_server");
        const auto& realtime = config_.service("realtime_gateway_server");
        std::ostringstream body;
        body << "{\"success\":true"
             << ",\"game_gateway\":{\"host\":\"" << json_escape(game.host)
             << "\",\"tcp_port\":" << game.tcp_port << "}"
             << ",\"realtime_gateway\":{\"host\":\""
             << json_escape(realtime.host)
             << "\",\"udp_kcp_port\":" << realtime.udp_kcp_port
             << ",\"enabled\":false,\"status\":\"reserved\"}"
             << "}";
        return json_response(
            http::status::ok, body.str(), request.version(), request.keep_alive());
    }

    http::response<http::string_body> handle_login(
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

    const mmo::runtime::foundation::ServerConfig& config_;
    mmo::runtime::routing::GatewayForwarder& forwarder_;
    mmo::runtime::observability::MetricsRegistry& metrics_;
    std::atomic<std::uint64_t> next_request_id_{1};
};

void handle_session(tcp::socket socket, ApiHandler& handler) {
    try {
        beast::flat_buffer buffer;
        for (;;) {
            http::request<http::string_body> request;
            http::read(socket, buffer, request);
            auto response = handler.handle(request);
            const bool close = response.need_eof();
            http::write(socket, response);
            if (close) {
                break;
            }
        }
        beast::error_code ignored;
        socket.shutdown(tcp::socket::shutdown_send, ignored);
    } catch (const std::exception&) {
    }
}

}  // namespace

int main() {
    try {
        mmo::runtime::foundation::ServerApp app("api_gateway_server");
        const auto tcp_options =
            mmo::runtime::transport::make_transport_options(
                app.config().transport.tcp, app.config().execution);
        mmo::runtime::routing::GatewayForwarder forwarder(
            app.service_name(), app.config(), tcp_options);
        mmo::runtime::observability::MetricsRegistry metrics;
        ApiHandler handler(app.config(), forwarder, metrics);

        boost::asio::io_context io_context;
        tcp::acceptor acceptor(
            io_context,
            tcp::endpoint(tcp::v4(), app.service_config().tcp_port));

        mmo::runtime::observability::log_info(
            mmo::runtime::observability::LogContext{app.service_name()},
            "http_server_listening port=" +
                std::to_string(app.service_config().tcp_port));

        for (;;) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);
            std::thread(handle_session, std::move(socket), std::ref(handler))
                .detach();
        }
    } catch (const std::exception& error) {
        mmo::runtime::observability::log_error(
            mmo::runtime::observability::LogContext{"api_gateway_server"},
            std::string("http_server_fatal error=") + error.what());
        return 1;
    }
}

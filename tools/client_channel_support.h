#pragma once

#include "runtime/foundation/config/simple_config.h"
#include "runtime/foundation/error/error_code.h"
#include "runtime/protocol/message_id.h"
#include "runtime/protocol/proto_codec.h"
#include "runtime/protocol/proto_mapper.h"
#include "runtime/transport/tls_options.h"
#include "runtime/transport/transport_client.h"

#include "game_backend.pb.h"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tools::client {

struct ProtoCallResult {
    bool ok = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    common::net::Packet packet;
};

struct TcpClientProfile {
    std::string host = "127.0.0.1";
    int port = 7000;
    int timeout_ms = 3000;
    framework::transport::TlsOptions tls;
};

struct HttpClientProfile {
    std::string host = "127.0.0.1";
    int port = 8080;
    int timeout_ms = 3000;
};

inline TcpClientProfile BuildTcpClientProfile(const common::config::SimpleConfig& config) {
    TcpClientProfile profile;
    profile.host = config.GetString("client.host", "127.0.0.1");
    profile.port = config.GetInt("port", config.GetInt("service.listen.port", 7000));
    profile.timeout_ms = config.GetInt("client.timeout_ms", 3000);
    profile.tls = framework::transport::ReadTlsOptions(config, "client.tls.");
    if (profile.tls.server_name.empty()) {
        profile.tls.server_name = profile.host;
    }
    return profile;
}

inline HttpClientProfile BuildHttpClientProfile(const common::config::SimpleConfig& config) {
    HttpClientProfile profile;
    profile.host = config.GetString("client.host", "127.0.0.1");
    profile.port = config.GetInt("port", config.GetInt("http.listen.port", 8080));
    profile.timeout_ms = config.GetInt("client.timeout_ms", 3000);
    return profile;
}

inline std::string ToLower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

inline common::error::ErrorCode MapHttpStatusToError(unsigned status) {
    if (status == 400) {
        return common::error::ErrorCode::kBadGateway;
    }
    if (status == 404 || status == 405) {
        return common::error::ErrorCode::kMessageNotSupported;
    }
    if (status >= 500) {
        return common::error::ErrorCode::kServiceUnavailable;
    }
    return common::error::ErrorCode::kBadGateway;
}

template <typename RequestT>
ProtoCallResult SendGateProto(framework::transport::TransportClient& client,
                              common::net::MessageId message_id,
                              const common::net::RequestContext& context,
                              RequestT* request) {
    common::net::FillProto(context, request->mutable_context());
    const auto packet = common::net::BuildPacket(message_id, context.request_id, *request);

    ProtoCallResult result;
    std::string error_message;
    if (!client.SendAndReceive(packet, &result.packet, &error_message)) {
        result.error_code = error_message == "timeout" ? common::error::ErrorCode::kUpstreamTimeout
                                                        : common::error::ErrorCode::kServiceUnavailable;
        result.error_message = error_message;
        return result;
    }

    const auto maybe_response_id = common::net::MessageIdFromInt(result.packet.header.msg_id);
    if (!maybe_response_id.has_value()) {
        result.error_code = common::error::ErrorCode::kBadGateway;
        result.error_message = "unknown response message id";
        return result;
    }

    if (*maybe_response_id == common::net::MessageId::kErrorResponse) {
        game_backend::proto::ErrorResponse error_response;
        if (!common::net::ParseMessage(result.packet.body, &error_response)) {
            result.error_code = common::error::ErrorCode::kBadGateway;
            result.error_message = "failed to parse error response";
            return result;
        }
        result.error_code = static_cast<common::error::ErrorCode>(error_response.error_code());
        result.error_message = error_response.error_message();
        return result;
    }

    result.ok = true;
    return result;
}

template <typename RequestT>
ProtoCallResult SendHttpProto(const HttpClientProfile& profile,
                              std::string_view target,
                              common::net::MessageId request_message_id,
                              const common::net::RequestContext& context,
                              const std::string& auth_token,
                              RequestT* request) {
    namespace asio = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    using tcp = asio::ip::tcp;

    common::net::FillProto(context, request->mutable_context());

    std::string request_body;
    ProtoCallResult result;
    if (!request->SerializeToString(&request_body)) {
        result.error_code = common::error::ErrorCode::kBadGateway;
        result.error_message = "failed to serialize request";
        return result;
    }

    beast::error_code ec;
    asio::io_context io_context;
    tcp::resolver resolver(io_context);
    beast::tcp_stream stream(io_context);
    stream.expires_after(std::chrono::milliseconds(profile.timeout_ms));

    const auto resolved = resolver.resolve(profile.host, std::to_string(profile.port), ec);
    if (ec) {
        result.error_code = common::error::ErrorCode::kServiceUnavailable;
        result.error_message = "failed to resolve api endpoint";
        return result;
    }

    stream.connect(resolved, ec);
    if (ec) {
        result.error_code = common::error::ErrorCode::kServiceUnavailable;
        result.error_message = "failed to connect api endpoint";
        return result;
    }

    http::request<http::string_body> http_request{http::verb::post, std::string(target), 11};
    http_request.set(http::field::host, profile.host);
    http_request.set(http::field::user_agent, "mobile_game_backend_demo_client");
    http_request.set(http::field::content_type, "application/x-protobuf");
    http_request.set("x-trace-id", context.trace_id);
    if (!auth_token.empty()) {
        http_request.set("x-auth-token", auth_token);
    }
    http_request.body() = std::move(request_body);
    http_request.prepare_payload();

    http::write(stream, http_request, ec);
    if (ec) {
        result.error_code = common::error::ErrorCode::kServiceUnavailable;
        result.error_message = "failed to send http request";
        return result;
    }

    beast::flat_buffer buffer;
    http::response<http::string_body> http_response;
    http::read(stream, buffer, http_response, ec);
    if (ec) {
        result.error_code = common::error::ErrorCode::kBadGateway;
        result.error_message = "failed to read http response";
        return result;
    }

    beast::error_code ignored;
    stream.socket().shutdown(tcp::socket::shutdown_both, ignored);

    if (http_response.result_int() != 200) {
        result.error_code = MapHttpStatusToError(http_response.result_int());
        result.error_message = http_response.body();
        return result;
    }

    std::unordered_map<std::string, std::string> headers;
    for (const auto& header : http_response.base()) {
        headers.emplace(ToLower(std::string(header.name_string())), std::string(header.value()));
    }

    const auto message_id_iter = headers.find("x-proto-message-id");
    if (message_id_iter == headers.end()) {
        result.error_code = common::error::ErrorCode::kBadGateway;
        result.error_message = "missing x-proto-message-id";
        return result;
    }

    const auto maybe_response_id =
        common::net::MessageIdFromInt(static_cast<std::uint32_t>(std::stoul(message_id_iter->second)));
    if (!maybe_response_id.has_value()) {
        result.error_code = common::error::ErrorCode::kBadGateway;
        result.error_message = "unknown response message id";
        return result;
    }

    result.packet.header.msg_id = static_cast<std::uint32_t>(*maybe_response_id);
    result.packet.header.request_id = context.request_id;
    result.packet.body = http_response.body();

    if (*maybe_response_id == common::net::MessageId::kErrorResponse) {
        game_backend::proto::ErrorResponse error_response;
        if (!common::net::ParseMessage(result.packet.body, &error_response)) {
            result.error_code = common::error::ErrorCode::kBadGateway;
            result.error_message = "failed to parse error response";
            return result;
        }
        result.error_code = static_cast<common::error::ErrorCode>(error_response.error_code());
        result.error_message = error_response.error_message();
        return result;
    }

    const auto expected_response_id = common::net::ExpectedResponseMessageId(request_message_id);
    if (expected_response_id.has_value() && *maybe_response_id != *expected_response_id) {
        result.error_code = common::error::ErrorCode::kBadGateway;
        result.error_message = "unexpected http response message id";
        return result;
    }

    result.ok = true;
    return result;
}

}  // namespace tools::client

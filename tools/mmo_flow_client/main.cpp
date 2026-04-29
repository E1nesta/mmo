#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

#include "public/auth.pb.h"
#include "public/gateway.pb.h"
#include "public/instance.pb.h"
#include "public/social.pb.h"
#include "public/world.pb.h"
#include "runtime/foundation/server_config.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/tcp_envelope_client.h"

namespace {

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

mmo::common::RequestContext make_context(
    std::uint64_t request_id,
    std::int64_t account_id = 0,
    std::int64_t player_id = 0,
    const std::string& session_token = {}) {
    mmo::common::RequestContext context;
    context.set_request_id(request_id);
    context.set_account_id(account_id);
    context.set_player_id(player_id);
    context.set_session_token(session_token);
    context.set_trace_id("local-flow-" + std::to_string(request_id));
    return context;
}

bool context_ok(const mmo::common::ResponseContext& context) {
    if (context.success()) {
        return true;
    }
    std::cerr << "request failed: " << context.error_code() << " "
              << context.error_message() << '\n';
    return false;
}

std::optional<std::string> http_post_form(
    const std::string& host,
    std::uint16_t port,
    const std::string& target,
    const std::string& body) {
    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        beast::tcp_stream stream(io_context);
        const auto endpoints = resolver.resolve(host, std::to_string(port));
        stream.connect(endpoints);

        http::request<http::string_body> request{
            http::verb::post, target, 11};
        request.set(http::field::host, host);
        request.set(http::field::user_agent, "mmo-flow-client");
        request.set(
            http::field::content_type,
            "application/x-www-form-urlencoded");
        request.body() = body;
        request.prepare_payload();

        http::write(stream, request);
        beast::flat_buffer buffer;
        http::response<http::string_body> response;
        http::read(stream, buffer, response);

        beast::error_code ignored;
        stream.socket().shutdown(tcp::socket::shutdown_both, ignored);

        if (response.result() != http::status::ok) {
            std::cerr << "http login failed status="
                      << static_cast<unsigned>(response.result_int())
                      << " body=" << response.body() << '\n';
            return std::nullopt;
        }
        return response.body();
    } catch (const std::exception& error) {
        std::cerr << "http login failed: " << error.what() << '\n';
        return std::nullopt;
    }
}

std::optional<std::string> json_string(
    const std::string& body,
    const std::string& key) {
    const std::string marker = "\"" + key + "\":\"";
    const std::size_t begin = body.find(marker);
    if (begin == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t value_begin = begin + marker.size();
    std::string value;
    for (std::size_t index = value_begin; index < body.size(); ++index) {
        if (body[index] == '"' && (index == value_begin || body[index - 1] != '\\')) {
            return value;
        }
        value.push_back(body[index]);
    }
    return std::nullopt;
}

std::optional<std::int64_t> json_int64(
    const std::string& body,
    const std::string& key) {
    const std::string marker = "\"" + key + "\":";
    const std::size_t begin = body.find(marker);
    if (begin == std::string::npos) {
        return std::nullopt;
    }
    const std::size_t value_begin = begin + marker.size();
    std::size_t value_end = value_begin;
    while (value_end < body.size() &&
           (body[value_end] == '-' ||
            (body[value_end] >= '0' && body[value_end] <= '9'))) {
        ++value_end;
    }
    if (value_end == value_begin) {
        return std::nullopt;
    }
    try {
        return std::stoll(body.substr(value_begin, value_end - value_begin));
    } catch (...) {
        return std::nullopt;
    }
}

bool expect_error_response(
    const mmo::common::Envelope& envelope,
    int expected_error_code) {
    if (envelope.message_type() != mmo::runtime::protocol::kErrorResponse) {
        std::cerr << "unexpected success response type: "
                  << envelope.message_type() << '\n';
        return false;
    }
    mmo::common::ResponseContext context;
    if (!mmo::runtime::protocol::unpack_message(envelope, context)) {
        std::cerr << "failed to parse error response payload\n";
        return false;
    }
    if (context.success() || context.error_code() != expected_error_code) {
        std::cerr << "unexpected error response: " << context.error_code()
                  << " " << context.error_message() << '\n';
        return false;
    }
    return true;
}

template <typename Response>
bool parse_response(
    const mmo::common::Envelope& envelope,
    const std::string& expected_type,
    Response& response) {
    if (envelope.message_type() != expected_type) {
        std::cerr << "unexpected response type: " << envelope.message_type()
                  << ", expected: " << expected_type << '\n';
        return false;
    }
    if (!mmo::runtime::protocol::unpack_message(envelope, response)) {
        std::cerr << "failed to parse response payload: " << expected_type << '\n';
        return false;
    }
    return context_ok(response.context());
}

}  // namespace

int main() {
    const auto config = mmo::runtime::foundation::load_server_config_from_env();
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(config.transport.tcp);
    mmo::runtime::transport::TcpEnvelopeClient client(tcp_options);
    const auto api_endpoint = config.service("api_gateway_server");
    const auto gateway_endpoint =
        mmo::runtime::transport::make_transport_endpoint(
            config.service("game_gateway_server"));

    const auto login_body = http_post_form(
        api_endpoint.host,
        api_endpoint.tcp_port,
        "/v1/login",
        "account_name=demo_player&password=demo_password&device_id=local-flow");
    if (!login_body.has_value()) {
        return 1;
    }

    mmo::public_api::LoginResponse login_response;
    const auto account_id = json_int64(*login_body, "account_id");
    const auto player_id = json_int64(*login_body, "player_id");
    const auto session_token = json_string(*login_body, "session_token");
    const auto access_token = json_string(*login_body, "access_token");
    const auto gateway_ticket = json_string(*login_body, "gateway_ticket");
    const auto access_expires =
        json_int64(*login_body, "access_token_expires_at_epoch_millis");
    const auto ticket_expires =
        json_int64(*login_body, "gateway_ticket_expires_at_epoch_millis");
    if (!account_id.has_value() || !player_id.has_value() ||
        !session_token.has_value() || !access_token.has_value() ||
        !gateway_ticket.has_value() || !access_expires.has_value() ||
        !ticket_expires.has_value()) {
        std::cerr << "failed to parse http login response: " << *login_body << '\n';
        return 1;
    }
    login_response.set_account_id(*account_id);
    login_response.set_player_id(*player_id);
    login_response.set_session_token(*session_token);
    login_response.set_access_token(*access_token);
    login_response.set_gateway_ticket(*gateway_ticket);
    login_response.set_access_token_expires_at_epoch_millis(*access_expires);
    login_response.set_gateway_ticket_expires_at_epoch_millis(*ticket_expires);

    std::cout << "[auth] login ok player_id=" << login_response.player_id()
              << " account_id=" << login_response.account_id() << '\n';

    mmo::public_api::GateLoginRequest invalid_gate_request;
    *invalid_gate_request.mutable_context() = make_context(
        200,
        login_response.account_id(),
        login_response.player_id(),
        login_response.session_token());
    invalid_gate_request.set_player_id(login_response.player_id());
    invalid_gate_request.set_session_token(login_response.session_token());
    invalid_gate_request.set_device_id("local-flow");
    invalid_gate_request.set_gateway_ticket(
        login_response.gateway_ticket() + "-tampered");

    const auto invalid_gate_envelope = mmo::runtime::protocol::pack_message(
        mmo::runtime::protocol::kGateLoginRequest,
        invalid_gate_request.context(),
        invalid_gate_request);
    const auto invalid_gate_response =
        client.send(gateway_endpoint, invalid_gate_envelope);
    if (!expect_error_response(invalid_gate_response, 401)) {
        return 1;
    }
    std::cout << "[gateway] invalid gate ticket rejected\n";

    mmo::public_api::GateLoginRequest gate_request;
    *gate_request.mutable_context() = make_context(
        2,
        login_response.account_id(),
        login_response.player_id(),
        login_response.session_token());
    gate_request.set_player_id(login_response.player_id());
    gate_request.set_session_token(login_response.session_token());
    gate_request.set_device_id("local-flow");
    gate_request.set_gateway_ticket(login_response.gateway_ticket());

    const auto gate_envelope = mmo::runtime::protocol::pack_message(
        mmo::runtime::protocol::kGateLoginRequest,
        gate_request.context(),
        gate_request);
    const auto gate_response_envelope = client.send(gateway_endpoint, gate_envelope);

    mmo::public_api::GateLoginResponse gate_response;
    if (!parse_response(
            gate_response_envelope,
            mmo::runtime::protocol::kGateLoginResponse,
            gate_response)) {
        return 1;
    }

    std::cout << "[gateway] gate login ok connection_id="
              << gate_response.connection_id() << '\n';

    mmo::public_api::EnterWorldRequest world_request;
    *world_request.mutable_context() = make_context(
        3,
        login_response.account_id(),
        login_response.player_id(),
        login_response.session_token());
    world_request.set_preferred_map_id(1001);
    world_request.set_preferred_line_id(1);

    const auto world_envelope = mmo::runtime::protocol::pack_message(
        mmo::runtime::protocol::kEnterWorldRequest,
        world_request.context(),
        world_request);
    const auto world_response_envelope = client.send(
        gateway_endpoint, world_envelope);

    mmo::public_api::EnterWorldResponse world_response;
    if (!parse_response(
            world_response_envelope,
            mmo::runtime::protocol::kEnterWorldResponse,
            world_response)) {
        return 1;
    }

    std::cout << "[world] enter world ok map="
              << world_response.route().map_id()
              << " line=" << world_response.route().line_id()
              << " scene=" << world_response.route().scene_id() << '\n';
    std::cout << "[scene] enter scene ok entity_id="
              << world_response.scene_entity_id()
              << " pos=(" << world_response.spawn_position().x() << ","
              << world_response.spawn_position().y() << ","
              << world_response.spawn_position().z() << ")\n";

    mmo::public_api::EnterInstanceRequest enter_instance_request;
    *enter_instance_request.mutable_context() = make_context(
        4,
        login_response.account_id(),
        login_response.player_id(),
        login_response.session_token());
    enter_instance_request.set_dungeon_id(101);

    const auto enter_instance_envelope = mmo::runtime::protocol::pack_message(
        mmo::runtime::protocol::kEnterInstanceRequest,
        enter_instance_request.context(),
        enter_instance_request);
    const auto enter_instance_response_envelope =
        client.send(gateway_endpoint, enter_instance_envelope);

    mmo::public_api::EnterInstanceResponse enter_instance_response;
    if (!parse_response(
            enter_instance_response_envelope,
            mmo::runtime::protocol::kEnterInstanceResponse,
            enter_instance_response)) {
        return 1;
    }

    std::cout << "[instance] enter instance ok instance_id="
              << enter_instance_response.instance_id()
              << " boss_entity_id=" << enter_instance_response.boss_entity_id()
              << '\n';

    mmo::public_api::SettleInstanceRequest settle_instance_request;
    *settle_instance_request.mutable_context() = make_context(
        5,
        login_response.account_id(),
        login_response.player_id(),
        login_response.session_token());
    settle_instance_request.set_instance_id(
        enter_instance_response.instance_id());
    settle_instance_request.set_idempotency_key(
        "local-flow-settle-" +
        std::to_string(enter_instance_response.instance_id()));
    settle_instance_request.set_win(true);

    const auto settle_instance_envelope = mmo::runtime::protocol::pack_message(
        mmo::runtime::protocol::kSettleInstanceRequest,
        settle_instance_request.context(),
        settle_instance_request);
    const auto settle_instance_response_envelope =
        client.send(gateway_endpoint, settle_instance_envelope);

    mmo::public_api::SettleInstanceResponse settle_instance_response;
    if (!parse_response(
            settle_instance_response_envelope,
            mmo::runtime::protocol::kSettleInstanceResponse,
            settle_instance_response)) {
        return 1;
    }

    std::cout << "[instance] settle instance ok reward_grant_id="
              << settle_instance_response.reward_grant_id()
              << " rewards=" << settle_instance_response.rewards_size()
              << " duplicate=" << settle_instance_response.duplicate() << '\n';

    mmo::public_api::SocialBoundaryRequest social_request;
    *social_request.mutable_context() = make_context(
        6,
        login_response.account_id(),
        login_response.player_id(),
        login_response.session_token());
    social_request.set_target_player_id(login_response.player_id());

    const auto social_envelope = mmo::runtime::protocol::pack_message(
        mmo::runtime::protocol::kSocialBoundaryRequest,
        social_request.context(),
        social_request);
    const auto social_response_envelope =
        client.send(gateway_endpoint, social_envelope);

    mmo::public_api::SocialBoundaryResponse social_response;
    if (!parse_response(
            social_response_envelope,
            mmo::runtime::protocol::kSocialBoundaryResponse,
            social_response)) {
        return 1;
    }

    std::cout << "[social] social boundary ok friend="
              << social_response.friend_boundary_available()
              << " chat=" << social_response.chat_boundary_available()
              << " team=" << social_response.team_boundary_available() << '\n';

    std::cout
        << "flow ok: HTTP Login -> GateLogin -> EnterWorld -> EnterInstance"
        << " -> SettleInstance -> SocialBoundary\n";
    return 0;
}

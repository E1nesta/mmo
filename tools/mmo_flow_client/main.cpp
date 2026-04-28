#include <iostream>
#include <string>

#include "public/auth.pb.h"
#include "public/gateway.pb.h"
#include "public/world.pb.h"
#include "runtime/foundation/server_config.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/tcp_envelope_server.h"

namespace {

mmo::public_api::RequestContext make_context(
    std::uint64_t request_id,
    std::int64_t account_id = 0,
    std::int64_t player_id = 0,
    const std::string& session_token = {}) {
    mmo::public_api::RequestContext context;
    context.set_request_id(request_id);
    context.set_account_id(account_id);
    context.set_player_id(player_id);
    context.set_session_token(session_token);
    context.set_trace_id("local-flow-" + std::to_string(request_id));
    return context;
}

bool context_ok(const mmo::public_api::ResponseContext& context) {
    if (context.success()) {
        return true;
    }
    std::cerr << "request failed: " << context.error_code() << " "
              << context.error_message() << '\n';
    return false;
}

template <typename Response>
bool parse_response(
    const mmo::public_api::Envelope& envelope,
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

    mmo::public_api::LoginRequest login_request;
    *login_request.mutable_context() = make_context(1);
    login_request.set_account_name("demo_player");
    login_request.set_password("demo_password");
    login_request.set_device_id("local-flow");

    const auto login_envelope = mmo::runtime::protocol::pack_message(
        mmo::runtime::protocol::kLoginRequest,
        login_request.context(),
        login_request);
    const auto login_response_envelope = mmo::runtime::transport::send_envelope(
        config.network.upstream_host,
        config.service("auth_server").tcp_port,
        login_envelope,
        tcp_options);

    mmo::public_api::LoginResponse login_response;
    if (!parse_response(
            login_response_envelope,
            mmo::runtime::protocol::kLoginResponse,
            login_response)) {
        return 1;
    }

    std::cout << "[auth] login ok player_id=" << login_response.player_id()
              << " account_id=" << login_response.account_id() << '\n';

    mmo::public_api::GateLoginRequest gate_request;
    *gate_request.mutable_context() = make_context(
        2,
        login_response.account_id(),
        login_response.player_id(),
        login_response.session_token());
    gate_request.set_player_id(login_response.player_id());
    gate_request.set_session_token(login_response.session_token());
    gate_request.set_device_id("local-flow");

    const auto gate_envelope = mmo::runtime::protocol::pack_message(
        mmo::runtime::protocol::kGateLoginRequest,
        gate_request.context(),
        gate_request);
    const auto gate_response_envelope = mmo::runtime::transport::send_envelope(
        config.network.upstream_host,
        config.service("gateway_server").tcp_port,
        gate_envelope,
        tcp_options);

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
    const auto world_response_envelope = mmo::runtime::transport::send_envelope(
        config.network.upstream_host,
        config.service("gateway_server").tcp_port,
        world_envelope,
        tcp_options);

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

    std::cout << "flow ok: Login -> GateLogin -> EnterWorld -> EnterScene\n";
    return 0;
}

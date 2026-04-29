#include <iostream>
#include <string>

#include "common/context.pb.h"
#include "public/auth.pb.h"
#include "public/instance.pb.h"
#include "runtime/foundation/server_config.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/internal_auth.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_client.h"

namespace {

bool expect(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }
    std::cerr << message << '\n';
    return false;
}

mmo::common::RequestContext make_context(std::uint64_t request_id) {
    mmo::common::RequestContext context;
    context.set_request_id(request_id);
    context.set_account_id(10001);
    context.set_player_id(20001);
    context.set_session_token("probe-session");
    context.set_trace_id("internal-auth-probe-" + std::to_string(request_id));
    return context;
}

bool verify_signature_helpers() {
    constexpr const char* kSecret = "probe-internal-auth-secret";
    constexpr std::int64_t kNow = 100000;

    mmo::public_api::LoginRequest request;
    *request.mutable_context() = make_context(1);
    request.set_account_name("probe");
    request.set_device_id("probe-device");

    auto envelope = mmo::runtime::protocol::pack_message(
        mmo::runtime::protocol::kLoginRequest,
        request.context(),
        request);

    std::string error_message;
    if (!expect(
            mmo::runtime::protocol::sign_internal_envelope(
                &envelope,
                "game_gateway_server",
                kNow,
                kSecret,
                &error_message),
            "expected sign_internal_envelope to succeed: " + error_message)) {
        return false;
    }

    if (!expect(
            mmo::runtime::protocol::validate_internal_envelope(
                envelope,
                kSecret,
                10000,
                kNow,
                &error_message),
            "expected validate_internal_envelope to succeed: " + error_message)) {
        return false;
    }

    auto tampered = envelope;
    tampered.set_payload(tampered.payload() + "x");
    if (!expect(
            !mmo::runtime::protocol::validate_internal_envelope(
                tampered,
                kSecret,
                10000,
                kNow,
                &error_message),
            "expected tampered payload validation to fail")) {
        return false;
    }

    if (!expect(
            !mmo::runtime::protocol::validate_internal_envelope(
                envelope,
                kSecret,
                10,
                kNow + 1000,
                &error_message),
            "expected old timestamp validation to fail")) {
        return false;
    }

    return true;
}

template <typename Request>
bool expect_direct_rejected(
    mmo::runtime::transport::TcpEnvelopeClient& client,
    const mmo::runtime::transport::TransportEndpoint& endpoint,
    const std::string& message_type,
    const mmo::common::RequestContext& context,
    const Request& request,
    const std::string& service_name) {
    const auto request_envelope =
        mmo::runtime::protocol::pack_message(message_type, context, request);
    const auto response_envelope = client.send(endpoint, request_envelope);
    if (!expect(
            response_envelope.message_type() ==
                mmo::runtime::protocol::kErrorResponse,
            "expected direct " + service_name + " request to return error envelope")) {
        return false;
    }

    mmo::common::ResponseContext response;
    if (!expect(
            mmo::runtime::protocol::unpack_message(response_envelope, response),
            "expected direct " + service_name + " error payload to parse")) {
        return false;
    }
    return expect(
        !response.success() && response.error_code() == 401,
        "expected direct " + service_name + " request to be rejected with 401");
}

bool verify_direct_requests_are_rejected() {
    const auto config = mmo::runtime::foundation::load_server_config_from_env();
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(config.transport.tcp);
    mmo::runtime::transport::TcpEnvelopeClient client(tcp_options);

    mmo::public_api::LoginRequest login_request;
    *login_request.mutable_context() = make_context(100);
    login_request.set_account_name("probe");
    login_request.set_device_id("probe-device");

    if (!expect_direct_rejected(
            client,
            mmo::runtime::transport::make_transport_endpoint(
                config.service("auth_server")),
            mmo::runtime::protocol::kLoginRequest,
            login_request.context(),
            login_request,
            "auth_server")) {
        return false;
    }

    mmo::public_api::EnterInstanceRequest instance_request;
    *instance_request.mutable_context() = make_context(101);
    instance_request.set_dungeon_id(101);

    return expect_direct_rejected(
        client,
        mmo::runtime::transport::make_transport_endpoint(
            config.service("instance_server")),
        mmo::runtime::protocol::kEnterInstanceRequest,
        instance_request.context(),
        instance_request,
        "instance_server");
}

}  // namespace

int main(int argc, char* argv[]) {
    const bool direct_only =
        argc > 1 && std::string(argv[1]) == "--direct-only";
    if (!direct_only && !verify_signature_helpers()) {
        return 1;
    }
    if (!verify_direct_requests_are_rejected()) {
        return 1;
    }
    std::cout << "internal auth probe ok\n";
    return 0;
}

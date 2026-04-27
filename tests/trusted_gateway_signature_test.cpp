#include "runtime/protocol/proto_mapper.h"

#include <chrono>
#include <iostream>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

}  // namespace

int main() {
    common::net::RequestContext context;
    context.trace_id = "trace-42";
    context.request_id = 42;
    context.auth_token = "auth-token-42";
    context.player_id = 20001;
    context.account_id = 10001;

    game_backend::proto::LoadPlayerRequest request;
    common::net::FillProto(context, request.mutable_context());
    request.set_player_id(context.player_id);

    auto packet = common::net::BuildPacket(common::net::MessageId::kLoadPlayerRequest, context.request_id, request);
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    const std::string shared_secret = "trusted-gateway-test-secret";
    std::string error_message;

    if (!Expect(common::net::SignTrustedRequest(
                    common::net::MessageId::kLoadPlayerRequest,
                    now_ms.count(),
                    shared_secret,
                    &packet,
                    &error_message),
                "expected request signing to succeed: " + error_message)) {
        return 1;
    }

    if (!Expect(common::net::ValidateTrustedRequest(
                    common::net::MessageId::kLoadPlayerRequest,
                    10000,
                    shared_secret,
                    packet,
                    &error_message),
                "expected signed request validation to succeed: " + error_message)) {
        return 1;
    }

    game_backend::proto::LoadPlayerRequest tampered_request;
    if (!Expect(common::net::ParseMessage(packet.body, &tampered_request),
                "expected to parse signed request packet")) {
        return 1;
    }
    tampered_request.set_player_id(99999);
    auto tampered_packet =
        common::net::BuildPacket(common::net::MessageId::kLoadPlayerRequest, context.request_id, tampered_request);

    if (!Expect(!common::net::ValidateTrustedRequest(
                    common::net::MessageId::kLoadPlayerRequest,
                    10000,
                    shared_secret,
                    tampered_packet,
                    &error_message),
                "expected tampered request validation to fail")) {
        return 1;
    }

    game_backend::proto::GateLoginRequest gate_login_request;
    common::net::FillProto(context, gate_login_request.mutable_context());
    gate_login_request.set_auth_token(context.auth_token);
    gate_login_request.set_player_id(context.player_id);
    gate_login_request.set_session_id(context.auth_token);
    gate_login_request.set_line_no(1);
    gate_login_request.set_device_id("device-test");

    auto gate_login_packet =
        common::net::BuildPacket(common::net::MessageId::kGateLoginRequest, context.request_id + 1, gate_login_request);
    if (!Expect(!common::net::SignTrustedRequest(
                    common::net::MessageId::kGateLoginRequest,
                    now_ms.count(),
                    shared_secret,
                    &gate_login_packet,
                    &error_message),
                "expected gate login request signing to be rejected")) {
        return 1;
    }

    if (!Expect(!common::net::ValidateTrustedRequest(
                    common::net::MessageId::kGateLoginRequest,
                    10000,
                    shared_secret,
                    gate_login_packet,
                    &error_message),
                "expected gate login trusted validation to be rejected")) {
        return 1;
    }

    game_backend::proto::GateRelinkRequest gate_relink_request;
    common::net::FillProto(context, gate_relink_request.mutable_context());
    gate_relink_request.set_auth_token(context.auth_token);
    gate_relink_request.set_player_id(context.player_id);
    gate_relink_request.set_session_id(context.auth_token);
    gate_relink_request.set_line_no(1);
    gate_relink_request.set_device_id("device-test");

    auto gate_relink_packet = common::net::BuildPacket(
        common::net::MessageId::kGateRelinkRequest, context.request_id + 2, gate_relink_request);
    if (!Expect(!common::net::SignTrustedRequest(
                    common::net::MessageId::kGateRelinkRequest,
                    now_ms.count(),
                    shared_secret,
                    &gate_relink_packet,
                    &error_message),
                "expected gate relink request signing to be rejected")) {
        return 1;
    }

    if (!Expect(!common::net::ValidateTrustedRequest(
                    common::net::MessageId::kGateRelinkRequest,
                    10000,
                    shared_secret,
                    gate_relink_packet,
                    &error_message),
                "expected gate relink trusted validation to be rejected")) {
        return 1;
    }

    game_backend::proto::PublishGateNotificationRequest publish_request;
    common::net::FillProto(context, publish_request.mutable_context());
    publish_request.set_player_id(context.player_id);
    publish_request.set_type(22);
    publish_request.set_timestamp(now_ms.count());

    auto publish_packet = common::net::BuildPacket(
        common::net::MessageId::kPublishGateNotificationRequest, context.request_id + 3, publish_request);
    if (!Expect(common::net::SignTrustedRequest(
                    common::net::MessageId::kPublishGateNotificationRequest,
                    now_ms.count(),
                    shared_secret,
                    &publish_packet,
                    &error_message),
                "expected publish gate notification signing to succeed: " + error_message)) {
        return 1;
    }

    if (!Expect(common::net::ValidateTrustedRequest(
                    common::net::MessageId::kPublishGateNotificationRequest,
                    10000,
                    shared_secret,
                    publish_packet,
                    &error_message),
                "expected publish gate notification validation to succeed: " + error_message)) {
        return 1;
    }

    game_backend::proto::KickPlayerRequest kick_request;
    common::net::FillProto(context, kick_request.mutable_context());
    kick_request.set_player_id(context.player_id);
    kick_request.set_reason("duplicate-login");

    auto kick_packet =
        common::net::BuildPacket(common::net::MessageId::kKickPlayerRequest, context.request_id + 4, kick_request);
    if (!Expect(common::net::SignTrustedRequest(
                    common::net::MessageId::kKickPlayerRequest,
                    now_ms.count(),
                    shared_secret,
                    &kick_packet,
                    &error_message),
                "expected kick player signing to succeed: " + error_message)) {
        return 1;
    }

    if (!Expect(common::net::ValidateTrustedRequest(
                    common::net::MessageId::kKickPlayerRequest,
                    10000,
                    shared_secret,
                    kick_packet,
                    &error_message),
                "expected kick player validation to succeed: " + error_message)) {
        return 1;
    }

    return 0;
}

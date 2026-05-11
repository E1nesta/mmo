#pragma once

#include <cstdint>

#include "runtime/protocol/message_catalog.h"

namespace mmo::protocol {

struct MessageSpec {
    std::uint32_t id{};
    const char* type{};
    runtime::protocol::MessageDomain domain{};
    runtime::protocol::MessageTransportMask transports{};
    runtime::protocol::MessageMode mode{runtime::protocol::MessageMode::kCall};
};

inline constexpr runtime::protocol::MessageTransportMask
    kPublicReliableTransports =
        runtime::protocol::kMessageTransportTcp |
        runtime::protocol::kMessageTransportWebSocket |
        runtime::protocol::kMessageTransportInternal;
inline constexpr runtime::protocol::MessageTransportMask
    kPublicRealtimeTransports =
        runtime::protocol::kMessageTransportKcp |
        runtime::protocol::kMessageTransportInternal;

inline constexpr MessageSpec kLoginRequestSpec{
    10000, "mmo.cs.LoginRequest",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kLoginResponseSpec{
    10001, "mmo.cs.LoginResponse",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kReply};

inline constexpr MessageSpec kGateLoginRequestSpec{
    10010, "mmo.cs.GateLoginRequest",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kGateLoginResponseSpec{
    10011, "mmo.cs.GateLoginResponse",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kReply};
inline constexpr MessageSpec kPingRequestSpec{
    10012, "mmo.cs.PingRequest",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kPingResponseSpec{
    10013, "mmo.cs.PingResponse",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kReply};
inline constexpr MessageSpec kReconnectRequestSpec{
    10014, "mmo.cs.ReconnectRequest",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kReconnectResponseSpec{
    10015, "mmo.cs.ReconnectResponse",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kReply};

inline constexpr MessageSpec kEnterWorldRequestSpec{
    10100, "mmo.cs.EnterWorldRequest",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kEnterWorldResponseSpec{
    10101, "mmo.cs.EnterWorldResponse",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kReply};

inline constexpr MessageSpec kEnterSceneRequestSpec{
    10200, "mmo.cs.EnterSceneRequest",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kEnterSceneResponseSpec{
    10201, "mmo.cs.EnterSceneResponse",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kReply};
inline constexpr MessageSpec kMoveCommandSpec{
    20000, "mmo.cs.MoveCommand",
    runtime::protocol::MessageDomain::kPublicRealtime,
    kPublicRealtimeTransports,
    runtime::protocol::MessageMode::kCast};
inline constexpr MessageSpec kMoveResultSpec{
    20001, "mmo.cs.MoveResult",
    runtime::protocol::MessageDomain::kPublicRealtime,
    kPublicRealtimeTransports,
    runtime::protocol::MessageMode::kCast};

inline constexpr MessageSpec kCastSkillRequestSpec{
    20100, "mmo.cs.CastSkillRequest",
    runtime::protocol::MessageDomain::kPublicRealtime,
    kPublicRealtimeTransports,
    runtime::protocol::MessageMode::kCast};
inline constexpr MessageSpec kCastSkillResponseSpec{
    20101, "mmo.cs.CastSkillResponse",
    runtime::protocol::MessageDomain::kPublicRealtime,
    kPublicRealtimeTransports,
    runtime::protocol::MessageMode::kCast};

inline constexpr MessageSpec kEnterInstanceRequestSpec{
    10300, "mmo.cs.EnterInstanceRequest",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kEnterInstanceResponseSpec{
    10301, "mmo.cs.EnterInstanceResponse",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kReply};
inline constexpr MessageSpec kSettleInstanceRequestSpec{
    10302, "mmo.cs.SettleInstanceRequest",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kSettleInstanceResponseSpec{
    10303, "mmo.cs.SettleInstanceResponse",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kReply};

inline constexpr MessageSpec kApplyRewardRequestSpec{
    10400, "mmo.cs.ApplyRewardRequest",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kApplyRewardResponseSpec{
    10401, "mmo.cs.ApplyRewardResponse",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kReply};

inline constexpr MessageSpec kSocialBoundaryRequestSpec{
    10500, "mmo.cs.SocialBoundaryRequest",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kSocialBoundaryResponseSpec{
    10501, "mmo.cs.SocialBoundaryResponse",
    runtime::protocol::MessageDomain::kPublicReliable,
    kPublicReliableTransports,
    runtime::protocol::MessageMode::kReply};

inline constexpr MessageSpec kGatewayAuthLoginRequestSpec{
    50000, "mmo.ss.GatewayAuthLoginRequest",
    runtime::protocol::MessageDomain::kGatewayInternal,
    runtime::protocol::kMessageTransportInternal,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kGatewayAuthLoginResponseSpec{
    50001, "mmo.ss.GatewayAuthLoginResponse",
    runtime::protocol::MessageDomain::kGatewayInternal,
    runtime::protocol::kMessageTransportInternal,
    runtime::protocol::MessageMode::kReply};

inline constexpr MessageSpec kAllocateSceneEntityRequestSpec{
    30000, "mmo.ss.AllocateSceneEntityRequest",
    runtime::protocol::MessageDomain::kBackendInternal,
    runtime::protocol::kMessageTransportInternal,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kAllocateSceneEntityResponseSpec{
    30001, "mmo.ss.AllocateSceneEntityResponse",
    runtime::protocol::MessageDomain::kBackendInternal,
    runtime::protocol::kMessageTransportInternal,
    runtime::protocol::MessageMode::kReply};

inline constexpr MessageSpec kGrantInstanceRewardRequestSpec{
    30100, "mmo.ss.GrantInstanceRewardRequest",
    runtime::protocol::MessageDomain::kBackendInternal,
    runtime::protocol::kMessageTransportInternal,
    runtime::protocol::MessageMode::kCall};
inline constexpr MessageSpec kGrantInstanceRewardResponseSpec{
    30101, "mmo.ss.GrantInstanceRewardResponse",
    runtime::protocol::MessageDomain::kBackendInternal,
    runtime::protocol::kMessageTransportInternal,
    runtime::protocol::MessageMode::kReply};

inline constexpr std::uint32_t kLoginRequest = kLoginRequestSpec.id;
inline constexpr std::uint32_t kLoginResponse = kLoginResponseSpec.id;
inline constexpr std::uint32_t kGateLoginRequest = kGateLoginRequestSpec.id;
inline constexpr std::uint32_t kGateLoginResponse = kGateLoginResponseSpec.id;
inline constexpr std::uint32_t kPingRequest = kPingRequestSpec.id;
inline constexpr std::uint32_t kPingResponse = kPingResponseSpec.id;
inline constexpr std::uint32_t kReconnectRequest = kReconnectRequestSpec.id;
inline constexpr std::uint32_t kReconnectResponse = kReconnectResponseSpec.id;
inline constexpr std::uint32_t kEnterWorldRequest = kEnterWorldRequestSpec.id;
inline constexpr std::uint32_t kEnterWorldResponse = kEnterWorldResponseSpec.id;
inline constexpr std::uint32_t kEnterSceneRequest = kEnterSceneRequestSpec.id;
inline constexpr std::uint32_t kEnterSceneResponse = kEnterSceneResponseSpec.id;
inline constexpr std::uint32_t kMoveCommand = kMoveCommandSpec.id;
inline constexpr std::uint32_t kMoveResult = kMoveResultSpec.id;
inline constexpr std::uint32_t kCastSkillRequest = kCastSkillRequestSpec.id;
inline constexpr std::uint32_t kCastSkillResponse = kCastSkillResponseSpec.id;
inline constexpr std::uint32_t kEnterInstanceRequest =
    kEnterInstanceRequestSpec.id;
inline constexpr std::uint32_t kEnterInstanceResponse =
    kEnterInstanceResponseSpec.id;
inline constexpr std::uint32_t kSettleInstanceRequest =
    kSettleInstanceRequestSpec.id;
inline constexpr std::uint32_t kSettleInstanceResponse =
    kSettleInstanceResponseSpec.id;
inline constexpr std::uint32_t kApplyRewardRequest =
    kApplyRewardRequestSpec.id;
inline constexpr std::uint32_t kApplyRewardResponse =
    kApplyRewardResponseSpec.id;
inline constexpr std::uint32_t kSocialBoundaryRequest =
    kSocialBoundaryRequestSpec.id;
inline constexpr std::uint32_t kSocialBoundaryResponse =
    kSocialBoundaryResponseSpec.id;
inline constexpr std::uint32_t kGatewayAuthLoginRequest =
    kGatewayAuthLoginRequestSpec.id;
inline constexpr std::uint32_t kGatewayAuthLoginResponse =
    kGatewayAuthLoginResponseSpec.id;
inline constexpr std::uint32_t kAllocateSceneEntityRequest =
    kAllocateSceneEntityRequestSpec.id;
inline constexpr std::uint32_t kAllocateSceneEntityResponse =
    kAllocateSceneEntityResponseSpec.id;
inline constexpr std::uint32_t kGrantInstanceRewardRequest =
    kGrantInstanceRewardRequestSpec.id;
inline constexpr std::uint32_t kGrantInstanceRewardResponse =
    kGrantInstanceRewardResponseSpec.id;

inline void add_message(
    runtime::protocol::MessageCatalog* catalog,
    MessageSpec spec) {
    catalog->add({
        spec.id,
        spec.type,
        spec.domain,
        spec.transports,
        spec.mode});
}

inline runtime::protocol::MessageCatalog make_message_catalog() {
    runtime::protocol::MessageCatalog catalog;
    add_message(&catalog, kLoginRequestSpec);
    add_message(&catalog, kLoginResponseSpec);
    add_message(&catalog, kGateLoginRequestSpec);
    add_message(&catalog, kGateLoginResponseSpec);
    add_message(&catalog, kPingRequestSpec);
    add_message(&catalog, kPingResponseSpec);
    add_message(&catalog, kReconnectRequestSpec);
    add_message(&catalog, kReconnectResponseSpec);
    add_message(&catalog, kEnterWorldRequestSpec);
    add_message(&catalog, kEnterWorldResponseSpec);
    add_message(&catalog, kEnterSceneRequestSpec);
    add_message(&catalog, kEnterSceneResponseSpec);
    add_message(&catalog, kMoveCommandSpec);
    add_message(&catalog, kMoveResultSpec);
    add_message(&catalog, kCastSkillRequestSpec);
    add_message(&catalog, kCastSkillResponseSpec);
    add_message(&catalog, kEnterInstanceRequestSpec);
    add_message(&catalog, kEnterInstanceResponseSpec);
    add_message(&catalog, kSettleInstanceRequestSpec);
    add_message(&catalog, kSettleInstanceResponseSpec);
    add_message(&catalog, kApplyRewardRequestSpec);
    add_message(&catalog, kApplyRewardResponseSpec);
    add_message(&catalog, kSocialBoundaryRequestSpec);
    add_message(&catalog, kSocialBoundaryResponseSpec);
    add_message(&catalog, kGatewayAuthLoginRequestSpec);
    add_message(&catalog, kGatewayAuthLoginResponseSpec);
    add_message(&catalog, kAllocateSceneEntityRequestSpec);
    add_message(&catalog, kAllocateSceneEntityResponseSpec);
    add_message(&catalog, kGrantInstanceRewardRequestSpec);
    add_message(&catalog, kGrantInstanceRewardResponseSpec);
    return catalog;
}

}  // namespace mmo::protocol

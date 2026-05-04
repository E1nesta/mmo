#pragma once

namespace apps::protocol {

inline constexpr const char* kLoginRequest = "public.auth.LoginRequest";
inline constexpr const char* kLoginResponse = "public.auth.LoginResponse";

inline constexpr const char* kGateLoginRequest = "public.gateway.GateLoginRequest";
inline constexpr const char* kGateLoginResponse = "public.gateway.GateLoginResponse";
inline constexpr const char* kPingRequest = "public.gateway.PingRequest";
inline constexpr const char* kPingResponse = "public.gateway.PingResponse";
inline constexpr const char* kReconnectRequest = "public.gateway.ReconnectRequest";
inline constexpr const char* kReconnectResponse = "public.gateway.ReconnectResponse";

inline constexpr const char* kEnterWorldRequest = "public.world.EnterWorldRequest";
inline constexpr const char* kEnterWorldResponse = "public.world.EnterWorldResponse";

inline constexpr const char* kEnterSceneRequest = "public.scene.EnterSceneRequest";
inline constexpr const char* kEnterSceneResponse = "public.scene.EnterSceneResponse";
inline constexpr const char* kMoveCommand = "public.scene.MoveCommand";
inline constexpr const char* kMoveResult = "public.scene.MoveResult";

inline constexpr const char* kCastSkillRequest = "public.combat.CastSkillRequest";
inline constexpr const char* kCastSkillResponse = "public.combat.CastSkillResponse";

inline constexpr const char* kEnterInstanceRequest =
    "public.instance.EnterInstanceRequest";
inline constexpr const char* kEnterInstanceResponse =
    "public.instance.EnterInstanceResponse";
inline constexpr const char* kSettleInstanceRequest =
    "public.instance.SettleInstanceRequest";
inline constexpr const char* kSettleInstanceResponse =
    "public.instance.SettleInstanceResponse";

inline constexpr const char* kApplyRewardRequest = "public.player.ApplyRewardRequest";
inline constexpr const char* kApplyRewardResponse =
    "public.player.ApplyRewardResponse";

inline constexpr const char* kSocialBoundaryRequest =
    "public.social.SocialBoundaryRequest";
inline constexpr const char* kSocialBoundaryResponse =
    "public.social.SocialBoundaryResponse";

inline constexpr const char* kGatewayEnterWorldRequest =
    "internal.gateway_world.GatewayEnterWorldRequest";
inline constexpr const char* kGatewayEnterWorldResponse =
    "internal.gateway_world.GatewayEnterWorldResponse";

inline constexpr const char* kGatewayAuthLoginRequest =
    "internal.gateway_auth.GatewayAuthLoginRequest";
inline constexpr const char* kGatewayAuthLoginResponse =
    "internal.gateway_auth.GatewayAuthLoginResponse";

inline constexpr const char* kGatewayEnterInstanceRequest =
    "internal.gateway_instance.GatewayEnterInstanceRequest";
inline constexpr const char* kGatewayEnterInstanceResponse =
    "internal.gateway_instance.GatewayEnterInstanceResponse";
inline constexpr const char* kGatewaySettleInstanceRequest =
    "internal.gateway_instance.GatewaySettleInstanceRequest";
inline constexpr const char* kGatewaySettleInstanceResponse =
    "internal.gateway_instance.GatewaySettleInstanceResponse";

inline constexpr const char* kGatewayApplyRewardRequest =
    "internal.gateway_player.GatewayApplyRewardRequest";
inline constexpr const char* kGatewayApplyRewardResponse =
    "internal.gateway_player.GatewayApplyRewardResponse";

inline constexpr const char* kGatewaySocialBoundaryRequest =
    "internal.gateway_social.GatewaySocialBoundaryRequest";
inline constexpr const char* kGatewaySocialBoundaryResponse =
    "internal.gateway_social.GatewaySocialBoundaryResponse";

inline constexpr const char* kAllocateSceneEntityRequest =
    "internal.world_scene.AllocateSceneEntityRequest";
inline constexpr const char* kAllocateSceneEntityResponse =
    "internal.world_scene.AllocateSceneEntityResponse";

inline constexpr const char* kGrantInstanceRewardRequest =
    "internal.instance_player.GrantInstanceRewardRequest";
inline constexpr const char* kGrantInstanceRewardResponse =
    "internal.instance_player.GrantInstanceRewardResponse";

}  // namespace apps::protocol

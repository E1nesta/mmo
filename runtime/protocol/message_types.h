#pragma once

namespace mmo::runtime::protocol {

inline constexpr const char* kErrorResponse = "public.common.ErrorResponse";

inline constexpr const char* kLoginRequest = "public.auth.LoginRequest";
inline constexpr const char* kLoginResponse = "public.auth.LoginResponse";

inline constexpr const char* kGateLoginRequest = "public.gateway.GateLoginRequest";
inline constexpr const char* kGateLoginResponse = "public.gateway.GateLoginResponse";

inline constexpr const char* kEnterWorldRequest = "public.world.EnterWorldRequest";
inline constexpr const char* kEnterWorldResponse = "public.world.EnterWorldResponse";

inline constexpr const char* kEnterSceneRequest = "public.scene.EnterSceneRequest";
inline constexpr const char* kEnterSceneResponse = "public.scene.EnterSceneResponse";

inline constexpr const char* kCastSkillRequest = "public.combat.CastSkillRequest";
inline constexpr const char* kCastSkillResponse = "public.combat.CastSkillResponse";

inline constexpr const char* kEnterInstanceRequest = "public.instance.EnterInstanceRequest";
inline constexpr const char* kEnterInstanceResponse = "public.instance.EnterInstanceResponse";
inline constexpr const char* kSettleInstanceRequest = "public.instance.SettleInstanceRequest";
inline constexpr const char* kSettleInstanceResponse = "public.instance.SettleInstanceResponse";

inline constexpr const char* kApplyRewardRequest = "public.player.ApplyRewardRequest";
inline constexpr const char* kApplyRewardResponse = "public.player.ApplyRewardResponse";

inline constexpr const char* kSocialBoundaryRequest = "public.social.SocialBoundaryRequest";
inline constexpr const char* kSocialBoundaryResponse = "public.social.SocialBoundaryResponse";

}  // namespace mmo::runtime::protocol

#pragma once

#include <cstdint>

namespace mmo::runtime::foundation {

inline constexpr const char* kLocalhost = "127.0.0.1";

inline constexpr std::uint16_t kAuthServerPort = 4101;
inline constexpr std::uint16_t kGatewayServerPort = 4102;
inline constexpr std::uint16_t kWorldServerPort = 4103;
inline constexpr std::uint16_t kSceneServerPort = 4104;
inline constexpr std::uint16_t kInstanceServerPort = 4105;
inline constexpr std::uint16_t kPlayerServerPort = 4106;
inline constexpr std::uint16_t kSocialServerPort = 4107;

}  // namespace mmo::runtime::foundation

#pragma once

#ifndef MOBILE_GAME_BACKEND_VERSION
#define MOBILE_GAME_BACKEND_VERSION "0.1.0"
#endif

#include <string_view>

namespace common::build {

inline std::string_view Version() {
    return MOBILE_GAME_BACKEND_VERSION;
}

}  // namespace common::build

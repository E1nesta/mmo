#pragma once

#include <cstdint>
#include <string>

namespace mmo::modules::auth {

struct LoginResult {
    std::int64_t account_id{};
    std::int64_t player_id{};
    std::string session_token;
    std::int64_t expires_at_epoch_seconds{};
};

class AuthService {
public:
    LoginResult login(const std::string& account_name, const std::string& device_id) const;
};

}  // namespace mmo::modules::auth

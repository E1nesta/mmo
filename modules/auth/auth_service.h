#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "modules/auth/account_repository.h"
#include "modules/auth/player_identity_repository.h"

namespace mmo::modules::auth {

struct LoginResult {
    bool success{};
    int error_code{};
    std::string error_message;
    std::string internal_reason;
    std::int64_t account_id{};
    std::int64_t player_id{};
    std::string session_token;
    std::int64_t expires_at_epoch_seconds{};
};

class AuthService {
public:
    AuthService(
        std::shared_ptr<AccountRepository> accounts,
        std::shared_ptr<PlayerIdentityRepository> identities);

    LoginResult login(
        const std::string& account_name,
        const std::string& password,
        const std::string& device_id) const;

private:
    std::shared_ptr<AccountRepository> accounts_;
    std::shared_ptr<PlayerIdentityRepository> identities_;
};

}  // namespace mmo::modules::auth

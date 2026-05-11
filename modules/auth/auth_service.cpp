#include "modules/auth/auth_service.h"

#include <openssl/rand.h>

#include <chrono>
#include <utility>

#include "modules/auth/password_hasher.h"

namespace modules::auth {
namespace {

LoginResult failed_login(std::string internal_reason) {
    LoginResult result;
    result.success = false;
    result.error_code = 401;
    result.error_message = "login authentication failed";
    result.internal_reason = std::move(internal_reason);
    return result;
}

std::int64_t current_time_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string make_session_token() {
    unsigned char bytes[32];
    if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
        return {};
    }

    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string token;
    token.reserve(8 + sizeof(bytes) * 2);
    token.append("session-");
    for (const auto byte : bytes) {
        token.push_back(kHexDigits[(byte >> 4U) & 0x0FU]);
        token.push_back(kHexDigits[byte & 0x0FU]);
    }
    return token;
}

}  // namespace

AuthService::AuthService(
    std::shared_ptr<AccountRepository> accounts,
    std::shared_ptr<PlayerIdentityRepository> identities)
    : accounts_(std::move(accounts)), identities_(std::move(identities)) {}

LoginResult AuthService::login(
    const std::string& account_name,
    const std::string& password,
    const std::string& device_id) const {
    if (accounts_ == nullptr || identities_ == nullptr) {
        return failed_login("auth repositories are not initialized");
    }
    if (account_name.empty() || password.empty() || device_id.empty()) {
        return failed_login("login request is missing required fields");
    }

    std::string error_message;
    const auto account =
        accounts_->find_by_account_name(account_name, &error_message);
    if (!account.has_value()) {
        return failed_login(
            error_message.empty() ? "account is missing" : error_message);
    }
    if (account->status == AccountStatus::kBanned) {
        return failed_login("account is banned");
    }
    if (account->status == AccountStatus::kDeleted) {
        return failed_login("account is deleted");
    }

    PasswordHash expected;
    expected.hash_hex = account->password_hash;
    expected.salt_hex = account->password_salt;
    expected.iterations = account->password_iterations;
    if (!PasswordHasher::verify_password(password, expected)) {
        return failed_login("password is invalid");
    }

    const auto identity =
        identities_->find_primary_by_account_id(account->account_id, &error_message);
    if (!identity.has_value()) {
        return failed_login(
            error_message.empty() ? "player identity is missing" : error_message);
    }

    LoginResult result;
    result.success = true;
    result.account_id = account->account_id;
    result.player_id = identity->player_id;
    result.session_token = make_session_token();
    if (result.session_token.empty()) {
        return failed_login("session token generation failed");
    }
    result.expires_at_epoch_seconds = current_time_seconds() + 3600;
    return result;
}

}  // namespace modules::auth

#include "modules/auth/auth_service.h"

#include <chrono>
#include <functional>
#include <sstream>
#include <utility>

#include "modules/auth/password_hasher.h"

namespace mmo::modules::auth {
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

std::string make_session_token(
    std::int64_t account_id,
    std::int64_t player_id,
    const std::string& device_id) {
    const auto now = current_time_seconds();
    const auto seed =
        std::to_string(account_id) + ":" + std::to_string(player_id) + ":" +
        device_id + ":" + std::to_string(now);
    std::ostringstream stream;
    stream << "session-" << player_id << '-' << now << '-'
           << std::hex << std::hash<std::string>{}(seed);
    return stream.str();
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
    result.session_token =
        make_session_token(result.account_id, result.player_id, device_id);
    result.expires_at_epoch_seconds = current_time_seconds() + 3600;
    return result;
}

}  // namespace mmo::modules::auth

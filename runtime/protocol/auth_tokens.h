#pragma once

#include <cstdint>
#include <string>

namespace mmo::runtime::protocol {

enum class AuthTokenPurpose {
    kAccess,
    kGateway,
};

struct AuthTokenClaims {
    AuthTokenPurpose purpose{AuthTokenPurpose::kAccess};
    std::int64_t account_id{};
    std::int64_t player_id{};
    std::string session_token;
    std::int64_t expires_at_epoch_millis{};
    std::string nonce;
};

bool issue_auth_token(
    AuthTokenPurpose purpose,
    std::int64_t account_id,
    std::int64_t player_id,
    const std::string& session_token,
    std::int64_t now_epoch_millis,
    std::int64_t ttl_millis,
    const std::string& shared_secret,
    std::string* token,
    std::int64_t* expires_at_epoch_millis,
    std::string* error_message);

bool validate_auth_token(
    const std::string& token,
    AuthTokenPurpose expected_purpose,
    const std::string& shared_secret,
    std::int64_t now_epoch_millis,
    AuthTokenClaims* claims,
    std::string* error_message);

}  // namespace mmo::runtime::protocol

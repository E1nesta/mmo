#pragma once

#include <cstdint>
#include <string>

namespace runtime::protocol {

enum class AuthTokenPurpose {
    kAccess,
    kGateway,
    kReconnect,
};

struct AuthTokenClaims {
    std::string version;
    std::string key_id;
    std::string issuer;
    std::string audience;
    AuthTokenPurpose purpose{AuthTokenPurpose::kAccess};
    std::int64_t account_id{};
    std::int64_t player_id{};
    std::string session_token;
    std::int64_t issued_at_epoch_millis{};
    std::int64_t expires_at_epoch_millis{};
    std::string jti;
};

struct AuthTokenOptions {
    std::string issuer;
    std::string access_audience;
    std::string gateway_audience;
    std::string active_key_id;
    std::string active_shared_secret;
    std::string previous_key_id;
    std::string previous_shared_secret;
    std::int64_t previous_key_accept_millis{};
};

bool issue_auth_token(
    AuthTokenPurpose purpose,
    std::int64_t account_id,
    std::int64_t player_id,
    const std::string& session_token,
    std::int64_t now_epoch_millis,
    std::int64_t ttl_millis,
    const AuthTokenOptions& options,
    std::string* token,
    std::int64_t* expires_at_epoch_millis,
    std::string* error_message);

bool validate_auth_token(
    const std::string& token,
    AuthTokenPurpose expected_purpose,
    const std::string& expected_audience,
    const AuthTokenOptions& options,
    std::int64_t now_epoch_millis,
    AuthTokenClaims* claims,
    std::string* error_message);

}  // namespace runtime::protocol

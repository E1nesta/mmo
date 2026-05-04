#include "runtime/protocol/auth_tokens.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <array>
#include <random>
#include <sstream>
#include <string>

namespace runtime::protocol {
namespace {

constexpr const char* kTokenVersion = "v1";

std::string purpose_name(AuthTokenPurpose purpose) {
    switch (purpose) {
        case AuthTokenPurpose::kAccess:
            return "access";
        case AuthTokenPurpose::kGateway:
            return "gateway";
        case AuthTokenPurpose::kReconnect:
            return "reconnect";
    }
    return "unknown";
}

std::string audience_for(
    AuthTokenPurpose purpose,
    const AuthTokenOptions& options) {
    return purpose == AuthTokenPurpose::kAccess ? options.access_audience
                                                : options.gateway_audience;
}

bool parse_purpose(const std::string& value, AuthTokenPurpose* purpose) {
    if (value == "access") {
        *purpose = AuthTokenPurpose::kAccess;
        return true;
    }
    if (value == "gateway") {
        *purpose = AuthTokenPurpose::kGateway;
        return true;
    }
    if (value == "reconnect") {
        *purpose = AuthTokenPurpose::kReconnect;
        return true;
    }
    return false;
}

std::string to_hex(const unsigned char* data, std::size_t size) {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    std::string output;
    output.reserve(size * 2);
    for (std::size_t index = 0; index < size; ++index) {
        output.push_back(kHexDigits[(data[index] >> 4U) & 0x0FU]);
        output.push_back(kHexDigits[data[index] & 0x0FU]);
    }
    return output;
}

int from_hex_digit(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

bool from_hex(const std::string& input, std::string* output) {
    if ((input.size() % 2U) != 0U) {
        return false;
    }
    output->clear();
    output->reserve(input.size() / 2U);
    for (std::size_t index = 0; index < input.size(); index += 2U) {
        const int high = from_hex_digit(input[index]);
        const int low = from_hex_digit(input[index + 1U]);
        if (high < 0 || low < 0) {
            return false;
        }
        output->push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

void append_field(const std::string& field, std::string* output) {
    output->append(std::to_string(field.size()));
    output->push_back(':');
    output->append(field);
}

bool read_field(
    const std::string& payload,
    std::size_t* offset,
    std::string* field) {
    const std::size_t colon = payload.find(':', *offset);
    if (colon == std::string::npos || colon == *offset) {
        return false;
    }
    std::size_t length = 0;
    try {
        length = static_cast<std::size_t>(
            std::stoull(payload.substr(*offset, colon - *offset)));
    } catch (...) {
        return false;
    }
    const std::size_t begin = colon + 1U;
    if (begin + length > payload.size()) {
        return false;
    }
    *field = payload.substr(begin, length);
    *offset = begin + length;
    return true;
}

std::string canonical_payload(const AuthTokenClaims& claims) {
    std::string output;
    output.reserve(claims.session_token.size() + 240);
    append_field(claims.version, &output);
    append_field(claims.key_id, &output);
    append_field(claims.issuer, &output);
    append_field(claims.audience, &output);
    append_field(purpose_name(claims.purpose), &output);
    append_field(std::to_string(claims.account_id), &output);
    append_field(std::to_string(claims.player_id), &output);
    append_field(claims.session_token, &output);
    append_field(std::to_string(claims.issued_at_epoch_millis), &output);
    append_field(std::to_string(claims.expires_at_epoch_millis), &output);
    append_field(claims.jti, &output);
    return output;
}

bool parse_payload(const std::string& payload, AuthTokenClaims* claims) {
    std::array<std::string, 11> fields;
    std::size_t offset = 0;
    for (auto& field : fields) {
        if (!read_field(payload, &offset, &field)) {
            return false;
        }
    }
    if (offset != payload.size()) {
        return false;
    }

    AuthTokenPurpose purpose;
    if (!parse_purpose(fields[4], &purpose)) {
        return false;
    }

    try {
        claims->version = fields[0];
        claims->key_id = fields[1];
        claims->issuer = fields[2];
        claims->audience = fields[3];
        claims->purpose = purpose;
        claims->account_id = std::stoll(fields[5]);
        claims->player_id = std::stoll(fields[6]);
        claims->session_token = fields[7];
        claims->issued_at_epoch_millis = std::stoll(fields[8]);
        claims->expires_at_epoch_millis = std::stoll(fields[9]);
        claims->jti = fields[10];
    } catch (...) {
        return false;
    }
    return true;
}

bool compute_hmac_sha256(
    const std::string& payload,
    const std::string& shared_secret,
    std::string* signature) {
    unsigned int digest_length = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];
    if (HMAC(
            EVP_sha256(),
            shared_secret.data(),
            static_cast<int>(shared_secret.size()),
            reinterpret_cast<const unsigned char*>(payload.data()),
            payload.size(),
            digest,
            &digest_length) == nullptr) {
        return false;
    }
    *signature = to_hex(digest, digest_length);
    return true;
}

bool timing_safe_equal(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }
    return CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}

std::string make_jti(std::int64_t now_epoch_millis) {
    std::random_device random_device;
    std::mt19937_64 generator(random_device());
    std::uniform_int_distribution<std::uint64_t> distribution;
    std::ostringstream stream;
    stream << now_epoch_millis << '-' << std::hex << distribution(generator);
    return stream.str();
}

bool split_token(
    const std::string& token,
    std::string* payload_hex,
    std::string* signature) {
    const std::size_t first = token.find('.');
    const std::size_t second = first == std::string::npos
                                   ? std::string::npos
                                   : token.find('.', first + 1U);
    if (first == std::string::npos || second == std::string::npos ||
        token.find('.', second + 1U) != std::string::npos) {
        return false;
    }
    if (token.substr(0, first) != kTokenVersion) {
        return false;
    }
    *payload_hex = token.substr(first + 1U, second - first - 1U);
    *signature = token.substr(second + 1U);
    return !payload_hex->empty() && !signature->empty();
}

std::string secret_for_key_id(
    const std::string& key_id,
    const AuthTokenOptions& options) {
    if (key_id == options.active_key_id) {
        return options.active_shared_secret;
    }
    if (!options.previous_key_id.empty() &&
        key_id == options.previous_key_id) {
        return options.previous_shared_secret;
    }
    return {};
}

bool previous_key_is_acceptable(
    const AuthTokenClaims& claims,
    const AuthTokenOptions& options,
    std::int64_t now_epoch_millis) {
    if (claims.key_id != options.previous_key_id) {
        return true;
    }
    return options.previous_key_accept_millis > 0 &&
           now_epoch_millis >= claims.issued_at_epoch_millis &&
           now_epoch_millis - claims.issued_at_epoch_millis <=
               options.previous_key_accept_millis;
}

bool options_valid_for_issue(const AuthTokenOptions& options) {
    return !options.issuer.empty() && !options.active_key_id.empty() &&
           !options.active_shared_secret.empty() &&
           !options.access_audience.empty() && !options.gateway_audience.empty();
}

}  // namespace

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
    std::string* error_message) {
    if (token == nullptr || expires_at_epoch_millis == nullptr) {
        if (error_message != nullptr) {
            *error_message = "token output is null";
        }
        return false;
    }
    if (!options_valid_for_issue(options)) {
        if (error_message != nullptr) {
            *error_message = "auth token options are invalid";
        }
        return false;
    }
    if (account_id <= 0 || player_id <= 0 || session_token.empty() ||
        now_epoch_millis <= 0 || ttl_millis <= 0) {
        if (error_message != nullptr) {
            *error_message = "auth token claims are invalid";
        }
        return false;
    }

    AuthTokenClaims claims;
    claims.version = kTokenVersion;
    claims.key_id = options.active_key_id;
    claims.issuer = options.issuer;
    claims.audience = audience_for(purpose, options);
    claims.purpose = purpose;
    claims.account_id = account_id;
    claims.player_id = player_id;
    claims.session_token = session_token;
    claims.issued_at_epoch_millis = now_epoch_millis;
    claims.expires_at_epoch_millis = now_epoch_millis + ttl_millis;
    claims.jti = make_jti(now_epoch_millis);

    const std::string payload = canonical_payload(claims);
    std::string signature;
    if (!compute_hmac_sha256(payload, options.active_shared_secret, &signature)) {
        if (error_message != nullptr) {
            *error_message = "failed to compute auth token signature";
        }
        return false;
    }

    *token = std::string(kTokenVersion) + "." +
             to_hex(reinterpret_cast<const unsigned char*>(payload.data()),
                    payload.size()) +
             "." + signature;
    *expires_at_epoch_millis = claims.expires_at_epoch_millis;
    return true;
}

bool validate_auth_token(
    const std::string& token,
    AuthTokenPurpose expected_purpose,
    const std::string& expected_audience,
    const AuthTokenOptions& options,
    std::int64_t now_epoch_millis,
    AuthTokenClaims* claims,
    std::string* error_message) {
    if (claims == nullptr) {
        if (error_message != nullptr) {
            *error_message = "claims output is null";
        }
        return false;
    }
    if (token.empty() || now_epoch_millis <= 0 || expected_audience.empty()) {
        if (error_message != nullptr) {
            *error_message = "auth token input is invalid";
        }
        return false;
    }

    std::string payload_hex;
    std::string signature;
    if (!split_token(token, &payload_hex, &signature)) {
        if (error_message != nullptr) {
            *error_message = "auth token format is invalid";
        }
        return false;
    }

    std::string payload;
    if (!from_hex(payload_hex, &payload) || !parse_payload(payload, claims)) {
        if (error_message != nullptr) {
            *error_message = "auth token payload is invalid";
        }
        return false;
    }
    if (claims->version != kTokenVersion ||
        claims->purpose != expected_purpose ||
        claims->issuer != options.issuer ||
        claims->audience != expected_audience ||
        now_epoch_millis < claims->issued_at_epoch_millis ||
        now_epoch_millis > claims->expires_at_epoch_millis ||
        !previous_key_is_acceptable(*claims, options, now_epoch_millis)) {
        if (error_message != nullptr) {
            *error_message = "auth token claims are invalid";
        }
        return false;
    }

    const std::string shared_secret = secret_for_key_id(claims->key_id, options);
    std::string expected_signature;
    if (shared_secret.empty() ||
        !compute_hmac_sha256(payload, shared_secret, &expected_signature) ||
        !timing_safe_equal(signature, expected_signature)) {
        if (error_message != nullptr) {
            *error_message = "auth token signature is invalid";
        }
        return false;
    }
    return true;
}

}  // namespace runtime::protocol

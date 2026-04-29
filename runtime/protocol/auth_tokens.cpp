#include "runtime/protocol/auth_tokens.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <array>
#include <chrono>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace mmo::runtime::protocol {
namespace {

constexpr const char* kTokenVersion = "v1";

std::string purpose_name(AuthTokenPurpose purpose) {
    switch (purpose) {
        case AuthTokenPurpose::kAccess:
            return "access";
        case AuthTokenPurpose::kGateway:
            return "gateway";
    }
    return "unknown";
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
        const int low = from_hex_digit(input[index + 1]);
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
    const std::string length_text = payload.substr(*offset, colon - *offset);
    std::size_t length = 0;
    try {
        length = static_cast<std::size_t>(std::stoull(length_text));
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
    output.reserve(claims.session_token.size() + 160);
    append_field(purpose_name(claims.purpose), &output);
    append_field(std::to_string(claims.account_id), &output);
    append_field(std::to_string(claims.player_id), &output);
    append_field(claims.session_token, &output);
    append_field(std::to_string(claims.expires_at_epoch_millis), &output);
    append_field(claims.nonce, &output);
    return output;
}

bool parse_payload(const std::string& payload, AuthTokenClaims* claims) {
    std::array<std::string, 6> fields;
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
    if (!parse_purpose(fields[0], &purpose)) {
        return false;
    }

    try {
        claims->purpose = purpose;
        claims->account_id = std::stoll(fields[1]);
        claims->player_id = std::stoll(fields[2]);
        claims->session_token = fields[3];
        claims->expires_at_epoch_millis = std::stoll(fields[4]);
        claims->nonce = fields[5];
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

std::string make_nonce(std::int64_t now_epoch_millis) {
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

}  // namespace

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
    std::string* error_message) {
    if (token == nullptr || expires_at_epoch_millis == nullptr) {
        if (error_message != nullptr) {
            *error_message = "token output is null";
        }
        return false;
    }
    if (shared_secret.empty()) {
        if (error_message != nullptr) {
            *error_message = "auth token shared secret is empty";
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
    claims.purpose = purpose;
    claims.account_id = account_id;
    claims.player_id = player_id;
    claims.session_token = session_token;
    claims.expires_at_epoch_millis = now_epoch_millis + ttl_millis;
    claims.nonce = make_nonce(now_epoch_millis);

    const std::string payload = canonical_payload(claims);
    std::string signature;
    if (!compute_hmac_sha256(payload, shared_secret, &signature)) {
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
    const std::string& shared_secret,
    std::int64_t now_epoch_millis,
    AuthTokenClaims* claims,
    std::string* error_message) {
    if (claims == nullptr) {
        if (error_message != nullptr) {
            *error_message = "claims output is null";
        }
        return false;
    }
    if (token.empty() || shared_secret.empty() || now_epoch_millis <= 0) {
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

    std::string expected_signature;
    if (!compute_hmac_sha256(payload, shared_secret, &expected_signature) ||
        !timing_safe_equal(signature, expected_signature)) {
        if (error_message != nullptr) {
            *error_message = "auth token signature is invalid";
        }
        return false;
    }
    if (claims->purpose != expected_purpose) {
        if (error_message != nullptr) {
            *error_message = "auth token purpose mismatch";
        }
        return false;
    }
    if (now_epoch_millis > claims->expires_at_epoch_millis) {
        if (error_message != nullptr) {
            *error_message = "auth token is expired";
        }
        return false;
    }
    return true;
}

}  // namespace mmo::runtime::protocol

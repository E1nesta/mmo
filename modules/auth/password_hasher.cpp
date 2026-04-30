#include "modules/auth/password_hasher.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <array>
#include <cctype>
#include <string>

namespace mmo::modules::auth {
namespace {

constexpr int kPasswordHashBytes = 32;

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

std::string lowercase_hex(std::string value) {
    for (auto& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool timing_safe_equal(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }
    return CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}

}  // namespace

bool PasswordHasher::hash_password(
    const std::string& password,
    const std::string& salt_hex,
    int iterations,
    PasswordHash* output,
    std::string* error_message) {
    if (output == nullptr) {
        if (error_message != nullptr) {
            *error_message = "password hash output is null";
        }
        return false;
    }
    if (password.empty() || salt_hex.empty() || iterations <= 0) {
        if (error_message != nullptr) {
            *error_message = "password hash input is invalid";
        }
        return false;
    }

    std::string salt;
    if (!from_hex(salt_hex, &salt) || salt.empty()) {
        if (error_message != nullptr) {
            *error_message = "password salt is invalid";
        }
        return false;
    }

    std::array<unsigned char, kPasswordHashBytes> digest{};
    if (PKCS5_PBKDF2_HMAC(
            password.c_str(),
            static_cast<int>(password.size()),
            reinterpret_cast<const unsigned char*>(salt.data()),
            static_cast<int>(salt.size()),
            iterations,
            EVP_sha256(),
            static_cast<int>(digest.size()),
            digest.data()) != 1) {
        if (error_message != nullptr) {
            *error_message = "failed to hash password";
        }
        return false;
    }

    output->hash_hex = to_hex(digest.data(), digest.size());
    output->salt_hex = lowercase_hex(salt_hex);
    output->iterations = iterations;
    return true;
}

bool PasswordHasher::verify_password(
    const std::string& password,
    const PasswordHash& expected) {
    PasswordHash actual;
    std::string ignored;
    if (!hash_password(
            password,
            expected.salt_hex,
            expected.iterations,
            &actual,
            &ignored)) {
        return false;
    }
    return timing_safe_equal(actual.hash_hex, lowercase_hex(expected.hash_hex));
}

}  // namespace mmo::modules::auth

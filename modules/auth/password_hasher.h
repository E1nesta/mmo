#pragma once

#include <string>

namespace mmo::modules::auth {

struct PasswordHash {
    std::string hash_hex;
    std::string salt_hex;
    int iterations{};
};

class PasswordHasher {
public:
    static bool hash_password(
        const std::string& password,
        const std::string& salt_hex,
        int iterations,
        PasswordHash* output,
        std::string* error_message);

    static bool verify_password(
        const std::string& password,
        const PasswordHash& expected);
};

}  // namespace mmo::modules::auth

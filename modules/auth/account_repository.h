#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace mmo::modules::auth {

enum class AccountStatus {
    kNormal,
    kBanned,
    kDeleted,
};

struct AccountRecord {
    std::int64_t account_id{};
    std::string account_name;
    std::string password_hash;
    std::string password_salt;
    int password_iterations{};
    AccountStatus status{AccountStatus::kNormal};
};

class AccountRepository {
public:
    virtual ~AccountRepository() = default;

    virtual std::optional<AccountRecord> find_by_account_name(
        const std::string& account_name,
        std::string* error_message) = 0;
};

}  // namespace mmo::modules::auth

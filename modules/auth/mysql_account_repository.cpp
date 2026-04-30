#include "modules/auth/mysql_account_repository.h"

#include <cstdlib>
#include <sstream>
#include <utility>

namespace mmo::modules::auth {
namespace {

AccountStatus parse_status(const std::string& value) {
    if (value == "banned") {
        return AccountStatus::kBanned;
    }
    if (value == "deleted") {
        return AccountStatus::kDeleted;
    }
    return AccountStatus::kNormal;
}

std::string quote(
    const mmo::runtime::storage::MysqlClient& client,
    const std::string& value) {
    return "'" + client.escape_string(value) + "'";
}

}  // namespace

MysqlAccountRepository::MysqlAccountRepository(
    std::shared_ptr<mmo::runtime::storage::MysqlConnectionPool> pool)
    : pool_(std::move(pool)) {}

std::optional<AccountRecord> MysqlAccountRepository::find_by_account_name(
    const std::string& account_name,
    std::string* error_message) {
    if (pool_ == nullptr) {
        if (error_message != nullptr) {
            *error_message = "mysql account repository is not initialized";
        }
        return std::nullopt;
    }

    const auto client = pool_->acquire();
    std::ostringstream sql;
    sql << "SELECT account_id, account_name, password_hash, password_salt, "
        << "password_iterations, status FROM accounts WHERE account_name="
        << quote(*client, account_name) << " LIMIT 1";

    std::vector<std::vector<std::string>> rows;
    if (!client->query(sql.str(), &rows, error_message) || rows.empty()) {
        return std::nullopt;
    }

    const auto& row = rows.front();
    if (row.size() < 6) {
        if (error_message != nullptr) {
            *error_message = "account row is malformed";
        }
        return std::nullopt;
    }

    AccountRecord record;
    record.account_id = std::strtoll(row[0].c_str(), nullptr, 10);
    record.account_name = row[1];
    record.password_hash = row[2];
    record.password_salt = row[3];
    record.password_iterations = std::strtol(row[4].c_str(), nullptr, 10);
    record.status = parse_status(row[5]);
    return record;
}

}  // namespace mmo::modules::auth

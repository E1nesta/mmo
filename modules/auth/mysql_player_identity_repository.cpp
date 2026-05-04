#include "modules/auth/mysql_player_identity_repository.h"

#include <cstdlib>
#include <sstream>
#include <utility>

namespace modules::auth {

MysqlPlayerIdentityRepository::MysqlPlayerIdentityRepository(
    std::shared_ptr<runtime::storage::MysqlConnectionPool> pool)
    : pool_(std::move(pool)) {}

std::optional<PlayerIdentity>
MysqlPlayerIdentityRepository::find_primary_by_account_id(
    std::int64_t account_id,
    std::string* error_message) {
    if (pool_ == nullptr) {
        if (error_message != nullptr) {
            *error_message = "mysql player identity repository is not initialized";
        }
        return std::nullopt;
    }

    const auto client = pool_->acquire();
    std::ostringstream sql;
    sql << "SELECT account_id, player_id FROM player_identities "
        << "WHERE account_id=" << account_id
        << " AND deleted_at IS NULL ORDER BY is_primary DESC, player_id ASC LIMIT 1";

    std::vector<std::vector<std::string>> rows;
    if (!client->query(sql.str(), &rows, error_message) || rows.empty()) {
        return std::nullopt;
    }

    const auto& row = rows.front();
    if (row.size() < 2) {
        if (error_message != nullptr) {
            *error_message = "player identity row is malformed";
        }
        return std::nullopt;
    }

    PlayerIdentity identity;
    identity.account_id = std::strtoll(row[0].c_str(), nullptr, 10);
    identity.player_id = std::strtoll(row[1].c_str(), nullptr, 10);
    return identity;
}

}  // namespace modules::auth

#pragma once

#include <memory>

#include "modules/auth/player_identity_repository.h"
#include "runtime/storage/mysql_connection_pool.h"

namespace modules::auth {

class MysqlPlayerIdentityRepository final : public PlayerIdentityRepository {
public:
    explicit MysqlPlayerIdentityRepository(
        std::shared_ptr<runtime::storage::MysqlConnectionPool> pool);

    std::optional<PlayerIdentity> find_primary_by_account_id(
        std::int64_t account_id,
        std::string* error_message) override;

private:
    std::shared_ptr<runtime::storage::MysqlConnectionPool> pool_;
};

}  // namespace modules::auth

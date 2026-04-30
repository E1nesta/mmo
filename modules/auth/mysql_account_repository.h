#pragma once

#include <memory>

#include "modules/auth/account_repository.h"
#include "runtime/storage/mysql_connection_pool.h"

namespace mmo::modules::auth {

class MysqlAccountRepository final : public AccountRepository {
public:
    explicit MysqlAccountRepository(
        std::shared_ptr<mmo::runtime::storage::MysqlConnectionPool> pool);

    std::optional<AccountRecord> find_by_account_name(
        const std::string& account_name,
        std::string* error_message) override;

private:
    std::shared_ptr<mmo::runtime::storage::MysqlConnectionPool> pool_;
};

}  // namespace mmo::modules::auth

// 代码规范落地：基础设施层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/storage/mysql/mysql_client_pool.h"
#include "modules/login/application/account_repository.h"

namespace login_server::auth {

// 基于 MySQL 的实现：承接对应边界的数据读写与一致性约束。
class MySqlAccountRepository final : public AccountRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    MySqlAccountRepository(common::mysql::MySqlClientPool& account_mysql_pool,
                           common::mysql::MySqlClientPool& player_mysql_pool);
    explicit MySqlAccountRepository(common::mysql::MySqlClient& mysql_client);

    [[nodiscard]] std::optional<common::model::Account> FindByName(const std::string& account_name) const override;
    bool RecordLoginAudit(std::int64_t account_id,
                          bool success,
                          const std::string& risk_reason_digest,
                          const std::string& client_ip,
                          const std::string& device_id,
                          std::string* error_message = nullptr) override;
    bool UpdateLastLoginTime(std::int64_t account_id, std::string* error_message = nullptr) override;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] std::int64_t FindDefaultPlayerId(std::int64_t account_id) const;

    common::mysql::MySqlClientPool* mysql_pool_ = nullptr;
    common::mysql::MySqlClientPool* player_mysql_pool_ = nullptr;
    common::mysql::MySqlClient* mysql_client_ = nullptr;
};

}  // namespace login_server::auth

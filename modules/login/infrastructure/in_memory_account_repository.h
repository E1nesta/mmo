// 代码规范落地：基础设施层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/config/simple_config.h"
#include "modules/login/application/account_repository.h"

#include <unordered_map>

namespace login_server::auth {

class InMemoryAccountRepository final : public AccountRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    static InMemoryAccountRepository FromConfig(const common::config::SimpleConfig& config);

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
    explicit InMemoryAccountRepository(common::model::Account account);

    std::unordered_map<std::string, common::model::Account> accounts_;
};

}  // namespace login_server::auth

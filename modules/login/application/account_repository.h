// 代码规范落地：业务编排层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/login/domain/account.h"

#include <cstdint>
#include <optional>
#include <string>

namespace login_server::auth {

// 存储边界：隔离应用编排与底层读写实现。
class AccountRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    virtual ~AccountRepository() = default;

    [[nodiscard]] virtual std::optional<common::model::Account> FindByName(const std::string& account_name) const = 0;
    virtual bool RecordLoginAudit(std::int64_t account_id,
                                  bool success,
                                  const std::string& risk_reason_digest,
                                  const std::string& client_ip,
                                  const std::string& device_id,
                                  std::string* error_message = nullptr) = 0;
    virtual bool UpdateLastLoginTime(std::int64_t account_id, std::string* error_message = nullptr) = 0;
};

}  // namespace login_server::auth

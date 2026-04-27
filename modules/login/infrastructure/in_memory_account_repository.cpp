// 代码规范落地：基础设施层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/login/infrastructure/in_memory_account_repository.h"

#include "runtime/foundation/security/password_hasher.h"

namespace login_server::auth {

InMemoryAccountRepository InMemoryAccountRepository::FromConfig(const common::config::SimpleConfig& config) {
    common::model::Account account;
    account.account_id = config.GetInt("demo.account_id", 10001);
    account.account_name = config.GetString("demo.account_name", "demo");
    account.password_hash = config.GetString("demo.password_hash");
    if (account.password_hash.empty()) {
        const auto encoded_hash = common::security::PasswordHasher::BuildEncodedHash(
            config.GetString("demo.password", "demo123"),
            "demo-local-salt");
        account.password_hash = encoded_hash.value_or("");
    }
    account.default_player_id = config.GetInt("demo.default_player_id", 20001);
    account.enabled = true;
    account.realname_verified = true;
    return InMemoryAccountRepository(account);
}

// 状态读取：`FindByName` 负责加载上下文并返回稳定结果。
std::optional<common::model::Account> InMemoryAccountRepository::FindByName(const std::string& account_name) const {
    const auto iter = accounts_.find(account_name);
    if (iter == accounts_.end()) {
        return std::nullopt;
    }
    return iter->second;
}

bool InMemoryAccountRepository::RecordLoginAudit(std::int64_t /*account_id*/,
                                                 bool /*success*/,
                                                 const std::string& /*risk_reason_digest*/,
                                                 const std::string& /*client_ip*/,
                                                 const std::string& /*device_id*/,
                                                 std::string* /*error_message*/) {
    return true;
}

// 状态推进：`UpdateLastLoginTime` 执行写链或补偿并收敛状态变化。
bool InMemoryAccountRepository::UpdateLastLoginTime(std::int64_t account_id, std::string* error_message) {
    for (auto& [name, account] : accounts_) {
        if (account.account_id == account_id) {
            return true;
        }
    }
    if (error_message != nullptr) {
        *error_message = "account not found";
    }
    return false;
}

InMemoryAccountRepository::InMemoryAccountRepository(common::model::Account account) {
    accounts_.emplace(account.account_name, std::move(account));
}

}  // namespace login_server::auth

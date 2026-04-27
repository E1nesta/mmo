// 代码规范落地：业务编排层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/login/application/login_service.h"

#include "runtime/foundation/log/logger.h"
#include "runtime/foundation/security/password_hasher.h"

namespace login_server {

namespace {

LoginResponse BuildLoginError(common::error::ErrorCode error_code, std::string error_message) {
    return {false, error_code, std::move(error_message), {}, 0};
}

LoginResponse BuildLoginSuccess(common::model::Session session, std::int64_t default_player_id) {
    auto response = LoginResponse{};
    response.success = true;
    response.error_code = common::error::ErrorCode::kOk;
    response.default_player_id = default_player_id;
    response.session = std::move(session);
    return response;
}

void RecordLoginAudit(auth::AccountRepository& account_repository,
                      const common::model::Account& account,
                      bool success,
                      const std::string& risk_reason_digest,
                      const LoginRequest& request) {
    std::string error_message;
    (void)account_repository.RecordLoginAudit(
        account.account_id, success, risk_reason_digest, request.client_ip, request.device_id, &error_message);
}

}  // namespace

LoginService::LoginService(auth::AccountRepository& account_repository, common::session::SessionStore& session_repository)
    : account_repository_(account_repository), session_repository_(session_repository) {}

LoginResponse LoginService::Login(const LoginRequest& request) {
    const auto account = LoadAccount(request.account_name);
    if (!account.has_value()) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "login failed: account not found account_name=" + request.account_name);
        return BuildLoginError(common::error::ErrorCode::kAccountNotFound, "account not found");
    }

    if (const auto validation = ValidateAccount(*account, request); validation.has_value()) {
        return *validation;
    }

    return BuildSuccessResponse(*account, request);
}

std::optional<common::model::Account> LoginService::LoadAccount(const std::string& account_name) const {
    return account_repository_.FindByName(account_name);
}

// 输入校验：`ValidateAccount` 校验关键约束并在失败时快速返回。
std::optional<LoginResponse> LoginService::ValidateAccount(const common::model::Account& account,
                                                           const LoginRequest& request) const {
    if (!account.enabled) {
        RecordLoginAudit(account_repository_, account, false, "ACCOUNT_DISABLED", request);
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "login failed: account disabled account_id=" + std::to_string(account.account_id));
        return BuildLoginError(common::error::ErrorCode::kAccountDisabled, "account disabled");
    }
    if (account.login_banned) {
        RecordLoginAudit(account_repository_, account, false, "ACCOUNT_LOGIN_BANNED", request);
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "login failed: account login banned account_id=" + std::to_string(account.account_id));
        return BuildLoginError(common::error::ErrorCode::kAccountDisabled, "account login banned");
    }
    if (!account.realname_verified) {
        RecordLoginAudit(account_repository_, account, false, "ACCOUNT_REALNAME_NOT_VERIFIED", request);
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "login failed: account realname not verified account_id=" + std::to_string(account.account_id));
        return BuildLoginError(common::error::ErrorCode::kAccountDisabled, "account realname not verified");
    }

    // 输入校验：`VerifyPassword` 校验关键约束并在失败时快速返回。
    if (!common::security::PasswordHasher::VerifyPassword(request.password, account.password_hash)) {
        RecordLoginAudit(account_repository_, account, false, "INVALID_PASSWORD", request);
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "login failed: invalid password account_id=" + std::to_string(account.account_id));
        return BuildLoginError(common::error::ErrorCode::kInvalidPassword, "invalid password");
    }

    return std::nullopt;
}

LoginResponse LoginService::BuildSuccessResponse(const common::model::Account& account,
                                                 const LoginRequest& request) const {
    std::string error_message;
    account_repository_.RecordLoginAudit(
        account.account_id, true, "LOGIN_OK", request.client_ip, request.device_id, &error_message);
    account_repository_.UpdateLastLoginTime(account.account_id, &error_message);
    auto session = session_repository_.Create(account.account_id, account.default_player_id);
    if (!request.device_id.empty() && session_repository_.BindDeviceId(session.session_id, request.device_id)) {
        session.device_id = request.device_id;
    }
    common::log::Logger::Instance().Log(
        common::log::LogLevel::kInfo,
        "login success: account_id=" + std::to_string(account.account_id) + " player_id=" +
            std::to_string(account.default_player_id) + " session_id=" + session.session_id);
    return BuildLoginSuccess(std::move(session), account.default_player_id);
}

}  // namespace login_server

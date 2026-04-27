// 代码规范落地：业务编排层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/error/error_code.h"
#include "runtime/session/session.h"
#include "modules/login/application/account_repository.h"
#include "runtime/session/session_store.h"

#include <cstdint>
#include <optional>
#include <string>

namespace login_server {

// 应用模型：描述当前用例输入。
struct LoginRequest {
    std::string account_name;
    std::string password;
    std::string client_ip;
    std::string device_id;
};

// 应用结果：统一返回业务阶段输出。
struct LoginResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    common::model::Session session;
    std::int64_t default_player_id = 0;
};

// 应用服务：编排流程阶段并收敛状态变化。
class LoginService {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    LoginService(auth::AccountRepository& account_repository, common::session::SessionStore& session_repository);

    [[nodiscard]] LoginResponse Login(const LoginRequest& request);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] std::optional<common::model::Account> LoadAccount(const std::string& account_name) const;
    [[nodiscard]] std::optional<LoginResponse> ValidateAccount(const common::model::Account& account,
                                                               const LoginRequest& request) const;
    [[nodiscard]] LoginResponse BuildSuccessResponse(const common::model::Account& account,
                                                     const LoginRequest& request) const;

    auth::AccountRepository& account_repository_;
    common::session::SessionStore& session_repository_;
};

}  // namespace login_server

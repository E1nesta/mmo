// 代码规范落地：服务入口层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/login/application/login_service.h"
#include "modules/login/infrastructure/mysql_account_repository.h"
#include "runtime/session/redis_session_store.h"
#include "runtime/storage/mysql/mysql_client_pool.h"
#include "runtime/storage/redis/redis_client_pool.h"
#include "runtime/transport/service_app.h"

namespace services::auth {

class AuthServerApp : public framework::service::ServiceApp {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    AuthServerApp();

// 受保护扩展：为派生类保留可控扩展点。
protected:
    bool BuildDependencies(std::string* error_message) override;
    void RegisterRoutes() override;
    bool RequiresTrustedGateway() const override { return true; }

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    common::net::Packet HandleLoginRequest(const framework::protocol::HandlerContext& context,
                                           const common::net::Packet& packet) const;

    std::unique_ptr<common::mysql::MySqlClientPool> account_writer_mysql_pool_;
    std::unique_ptr<common::mysql::MySqlClientPool> player_writer_mysql_pool_;
    std::unique_ptr<common::redis::RedisClientPool> session_redis_pool_;
    std::unique_ptr<login_server::auth::MySqlAccountRepository> account_repository_;
    std::unique_ptr<common::session::RedisSessionStore> session_repository_;
    std::unique_ptr<login_server::LoginService> login_service_;
};

}  // namespace services::auth

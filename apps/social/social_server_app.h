// 代码规范落地：服务入口层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/social/application/social_service.h"
#include "modules/social/infrastructure/in_memory_social_repository.h"
#include "runtime/storage/mysql/mysql_client_pool.h"
#include "runtime/storage/redis/redis_client_pool.h"
#include "runtime/transport/service_app.h"

namespace services::social {

class SocialServerApp : public framework::service::ServiceApp {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    SocialServerApp();

// 受保护扩展：为派生类保留可控扩展点。
protected:
    bool BuildDependencies(std::string* error_message) override;
    void RegisterRoutes() override;
    bool RequiresTrustedGateway() const override { return true; }

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    common::net::Packet HandleListFriendsRequest(const framework::protocol::HandlerContext& context,
                                                 const common::net::Packet& packet) const;
    common::net::Packet HandleListConversationsRequest(const framework::protocol::HandlerContext& context,
                                                       const common::net::Packet& packet) const;
    common::net::Packet HandleGetChatHistoryRequest(const framework::protocol::HandlerContext& context,
                                                    const common::net::Packet& packet) const;

    std::unique_ptr<common::mysql::MySqlClientPool> social_writer_mysql_pool_;
    std::unique_ptr<common::redis::RedisClientPool> session_redis_pool_;
    std::unique_ptr<game_server::social::InMemorySocialRepository> social_repository_;
    std::unique_ptr<game_server::social::SocialService> social_service_;
};

}  // namespace services::social

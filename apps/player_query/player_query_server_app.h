// 代码规范落地：服务入口层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/player/application/player_query_service.h"
#include "modules/player/infrastructure/read_write_mysql_player_repository.h"
#include "modules/player/infrastructure/redis_player_cache_repository.h"
#include "runtime/storage/mysql/mysql_read_write_client_pool.h"
#include "runtime/storage/redis/redis_client_pool.h"
#include "runtime/transport/service_app.h"

namespace services::player_query {

class PlayerQueryServerApp : public framework::service::ServiceApp {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    PlayerQueryServerApp();

// 受保护扩展：为派生类保留可控扩展点。
protected:
    bool BuildDependencies(std::string* error_message) override;
    void RegisterRoutes() override;
    bool RequiresTrustedGateway() const override { return true; }

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    common::net::Packet HandlePlayerInitRequest(const framework::protocol::HandlerContext& context,
                                                const common::net::Packet& packet) const;
    common::net::Packet HandlePlayerSnapshotRequest(const framework::protocol::HandlerContext& context,
                                                    const common::net::Packet& packet) const;

    std::unique_ptr<common::mysql::MySqlReadWriteClientPool> mysql_pool_;
    std::unique_ptr<common::redis::RedisClientPool> player_cache_redis_pool_;
    std::unique_ptr<game_server::player::ReadWriteMySqlPlayerRepository> player_repository_;
    std::unique_ptr<game_server::player::RedisPlayerCacheRepository> player_cache_repository_;
    std::unique_ptr<game_server::player::PlayerQueryService> player_query_service_;
};

}  // namespace services::player_query

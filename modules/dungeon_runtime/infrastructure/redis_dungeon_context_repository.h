// 代码规范落地：基础设施层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/config/simple_config.h"
#include "runtime/storage/redis/redis_client_pool.h"
#include "modules/dungeon_runtime/ports/dungeon_context_repository.h"

namespace game_server::dungeon_runtime {

// 基于 Redis 的副本上下文存储边界实现。
class RedisDungeonContextRepository final : public DungeonContextRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    static RedisDungeonContextRepository FromConfig(common::redis::RedisClientPool& redis_pool,
                                                   const common::config::SimpleConfig& config);

    RedisDungeonContextRepository(common::redis::RedisClientPool& redis_pool, int ttl_seconds);

    bool Save(const DungeonContext& dungeon_context) override;
    [[nodiscard]] std::optional<DungeonContext> FindBySessionId(std::int64_t session_id) const override;
    bool Delete(std::int64_t session_id) override;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] static std::string CacheKey(std::int64_t session_id);

    common::redis::RedisClientPool& redis_pool_;
    int ttl_seconds_ = 3600;
};

}  // namespace game_server::dungeon_runtime

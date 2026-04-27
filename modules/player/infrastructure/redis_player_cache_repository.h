// 代码规范落地：基础设施层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/config/simple_config.h"
#include "runtime/storage/redis/redis_client_pool.h"
#include "modules/player/ports/player_cache_repository.h"

namespace game_server::player {

// 基于 Redis 的实现：承接对应边界的数据读写与状态维护。
class RedisPlayerCacheRepository final : public PlayerCacheRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    static RedisPlayerCacheRepository FromConfig(common::redis::RedisClientPool& redis_pool,
                                                 const common::config::SimpleConfig& config);

    RedisPlayerCacheRepository(common::redis::RedisClientPool& redis_pool, int ttl_seconds);

    bool Save(const common::model::PlayerState& player_state) override;
    [[nodiscard]] std::optional<common::model::PlayerState> FindByPlayerId(std::int64_t player_id) const override;
    bool Invalidate(std::int64_t player_id) override;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] static std::string CacheKey(std::int64_t player_id);
    [[nodiscard]] static std::string SerializeProgress(const common::model::PlayerState& player_state);
    [[nodiscard]] static std::vector<common::model::PlayerStageProgress> ParseProgress(const std::string& raw_value);

    common::redis::RedisClientPool& redis_pool_;
    int ttl_seconds_ = 300;
};

}  // namespace game_server::player

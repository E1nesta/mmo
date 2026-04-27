// 代码规范落地：基础设施层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/storage/redis/redis_client_pool.h"
#include "modules/dungeon_runtime/ports/player_lock_repository.h"

#include <cstdint>
#include <string>

namespace game_server::dungeon_runtime {

// 基于 Redis 的玩家锁边界实现。
class RedisPlayerLockRepository final : public PlayerLockRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    explicit RedisPlayerLockRepository(common::redis::RedisClientPool& redis_pool, int ttl_seconds = 10);

    bool Acquire(std::int64_t player_id) override;
    void Release(std::int64_t player_id) override;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] std::string PlayerLockKey(std::int64_t player_id) const;

    common::redis::RedisClientPool& redis_pool_;
    int ttl_seconds_ = 10;
};

}  // namespace game_server::dungeon_runtime

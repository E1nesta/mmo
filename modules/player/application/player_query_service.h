// 代码规范落地：业务编排层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/player/application/player_service.h"

#include <atomic>

namespace game_server::player {

class PlayerQueryService {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    PlayerQueryService(PlayerRepository& player_repository,
                       PlayerCacheRepository& player_cache_repository);

    [[nodiscard]] LoadPlayerResponse LoadPlayer(std::int64_t player_id);
    [[nodiscard]] PlayerSnapshotResponse GetPlayerSnapshot(std::int64_t player_id);
    [[nodiscard]] InvalidatePlayerCacheResponse InvalidatePlayerCache(std::int64_t player_id);
    [[nodiscard]] std::uint64_t CacheHitCount() const;
    [[nodiscard]] std::uint64_t CacheMissCount() const;
    [[nodiscard]] std::uint64_t StorageLoadCount() const;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] std::optional<common::model::PlayerState> LoadCachedPlayer(std::int64_t player_id) const;
    [[nodiscard]] std::optional<common::model::PlayerState> LoadPlayerFromStorage(std::int64_t player_id) const;
    void RefreshPlayerCacheBestEffort(std::int64_t player_id) const;

    // 读路径依赖：正式存储与查询缓存共同构成读链路。
    PlayerRepository& player_repository_;
    PlayerCacheRepository& player_cache_repository_;

    // 观测计数器：记录缓存命中、未命中与回源次数。
    mutable std::atomic<std::uint64_t> cache_hit_count_{0};
    mutable std::atomic<std::uint64_t> cache_miss_count_{0};
    mutable std::atomic<std::uint64_t> storage_load_count_{0};
};

}  // namespace game_server::player

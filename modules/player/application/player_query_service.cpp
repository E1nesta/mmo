// 代码规范落地：业务编排层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/player/application/player_query_service.h"

#include "runtime/foundation/log/logger.h"

namespace game_server::player {

namespace {

LoadPlayerResponse BuildLoadPlayerError(common::error::ErrorCode error_code, std::string error_message) {
    return {false, error_code, std::move(error_message), {}, false};
}

LoadPlayerResponse BuildLoadPlayerSuccess(const common::model::PlayerState& player_state, bool loaded_from_cache) {
    return {true, common::error::ErrorCode::kOk, "", common::model::BuildHomeInitView(player_state), loaded_from_cache};
}

PlayerSnapshotResponse BuildSnapshotSuccess(const common::model::PlayerState& player_state) {
    return {true,
            common::error::ErrorCode::kOk,
            "",
            true,
            player_state.profile.player_id,
            player_state.profile.level,
            player_state.profile.stamina,
            player_state.profile.nickname,
            player_state.profile.gold,
            player_state.profile.diamond,
            player_state.profile.main_stage_id,
            player_state.profile.main_chapter_id,
            player_state.profile.fight_power,
            player_state.currencies,
            player_state.role_summaries,
            static_cast<int>(player_state.stage_progress.size())};
}

PlayerSnapshotResponse BuildSnapshotMissing() {
    return {true, common::error::ErrorCode::kOk, "", false, 0, 0, 0, "", 0, 0, 0, 0, 0, {}, {}, 0};
}

InvalidatePlayerCacheResponse BuildInvalidateSuccess() {
    return {true, common::error::ErrorCode::kOk, ""};
}

InvalidatePlayerCacheResponse BuildInvalidateFailure(common::error::ErrorCode error_code, std::string error_message) {
    return {false, error_code, std::move(error_message)};
}

}  // namespace

PlayerQueryService::PlayerQueryService(PlayerRepository& player_repository,
                                       PlayerCacheRepository& player_cache_repository)
    : player_repository_(player_repository),
      player_cache_repository_(player_cache_repository) {}

// 状态读取：`LoadPlayer` 负责加载上下文并返回稳定结果。
LoadPlayerResponse PlayerQueryService::LoadPlayer(std::int64_t player_id) {
    // 入口阶段：优先尝试查询缓存，命中后直接返回快照视图。
    if (const auto cached_state = LoadCachedPlayer(player_id); cached_state.has_value()) {
        cache_hit_count_.fetch_add(1, std::memory_order_relaxed);
        return BuildLoadPlayerSuccess(*cached_state, true);
    }

    // 回源阶段：缓存未命中后读取正式存储。
    cache_miss_count_.fetch_add(1, std::memory_order_relaxed);
    storage_load_count_.fetch_add(1, std::memory_order_relaxed);
    const auto player_state = LoadPlayerFromStorage(player_id);
    if (!player_state.has_value()) {
        return BuildLoadPlayerError(common::error::ErrorCode::kPlayerNotFound, "player not found");
    }

    // 收敛阶段：将最新状态回填缓存，稳定后续查询延迟。
    player_cache_repository_.Save(*player_state);
    return BuildLoadPlayerSuccess(*player_state, false);
}

// 状态读取：`GetPlayerSnapshot` 负责加载上下文并返回稳定结果。
PlayerSnapshotResponse PlayerQueryService::GetPlayerSnapshot(std::int64_t player_id) {
    // 入口阶段：先尝试读取缓存中的玩家快照。
    if (const auto cached_state = LoadCachedPlayer(player_id); cached_state.has_value()) {
        cache_hit_count_.fetch_add(1, std::memory_order_relaxed);
        return BuildSnapshotSuccess(*cached_state);
    }

    // 回源阶段：未命中时回源正式存储。
    cache_miss_count_.fetch_add(1, std::memory_order_relaxed);
    storage_load_count_.fetch_add(1, std::memory_order_relaxed);
    const auto player_state = LoadPlayerFromStorage(player_id);
    if (!player_state.has_value()) {
        return BuildSnapshotMissing();
    }

    // 收敛阶段：缓存回填后再返回快照，保证读路径一致性。
    player_cache_repository_.Save(*player_state);
    return BuildSnapshotSuccess(*player_state);
}

// 状态推进：`InvalidatePlayerCache` 执行写链或补偿并收敛状态变化。
InvalidatePlayerCacheResponse PlayerQueryService::InvalidatePlayerCache(std::int64_t player_id) {
    // 状态阶段：主动失效缓存，强制下一次读取回源正式存储。
    if (player_cache_repository_.Invalidate(player_id)) {
        return BuildInvalidateSuccess();
    }

    return BuildInvalidateFailure(common::error::ErrorCode::kStorageError, "failed to invalidate player cache");
}

std::uint64_t PlayerQueryService::CacheHitCount() const {
    return cache_hit_count_.load(std::memory_order_relaxed);
}

std::uint64_t PlayerQueryService::CacheMissCount() const {
    return cache_miss_count_.load(std::memory_order_relaxed);
}

std::uint64_t PlayerQueryService::StorageLoadCount() const {
    return storage_load_count_.load(std::memory_order_relaxed);
}

std::optional<common::model::PlayerState> PlayerQueryService::LoadCachedPlayer(std::int64_t player_id) const {
    return player_cache_repository_.FindByPlayerId(player_id);
}

std::optional<common::model::PlayerState> PlayerQueryService::LoadPlayerFromStorage(std::int64_t player_id) const {
    return player_repository_.LoadPlayerState(player_id);
}

void PlayerQueryService::RefreshPlayerCacheBestEffort(std::int64_t player_id) const {
    // 回源阶段：尽力刷新缓存，失败时退化为失效处理。
    if (const auto player_state = LoadPlayerFromStorage(player_id); player_state.has_value()) {
        if (player_cache_repository_.Save(*player_state)) {
            return;
        }
        if (player_cache_repository_.Invalidate(player_id)) {
            return;
        }
    } else if (player_cache_repository_.Invalidate(player_id)) {
        return;
    }

    common::log::Logger::Instance().Log(
        common::log::LogLevel::kWarn,
        "player cache refresh failed after query refresh for player_id=" + std::to_string(player_id));
}

}  // namespace game_server::player

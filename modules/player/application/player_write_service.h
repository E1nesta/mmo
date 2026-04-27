// 代码规范落地：业务编排层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/player/domain/reward.h"
#include "modules/player/application/player_service.h"

namespace game_server::player {

class PlayerWriteService {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    PlayerWriteService(PlayerRepository& player_repository,
                       PlayerCacheRepository& player_cache_repository);

    [[nodiscard]] BattleEntrySnapshotResponse GetBattleEntrySnapshot(std::int64_t player_id);
    [[nodiscard]] PrepareBattleEntryResponse PrepareBattleEntry(std::int64_t player_id,
                                                                std::int64_t session_id,
                                                                int energy_cost,
                                                                const std::string& idempotency_key);
    [[nodiscard]] CancelBattleEntryResponse CancelBattleEntry(std::int64_t player_id,
                                                              std::int64_t session_id,
                                                              int energy_refund,
                                                              const std::string& idempotency_key);
    [[nodiscard]] ApplyRewardGrantResponse ApplyRewardGrant(std::int64_t player_id,
                                                            std::int64_t grant_id,
                                                            std::int64_t session_id,
                                                            const std::vector<common::model::Reward>& rewards,
                                                            const std::string& idempotency_key);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    void InvalidatePlayerCacheBestEffort(std::int64_t player_id) const;

    // 写路径依赖：正式状态写入与查询缓存失效配合推进一致性。
    PlayerRepository& player_repository_;
    PlayerCacheRepository& player_cache_repository_;
};

}  // namespace game_server::player

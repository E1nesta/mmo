// 代码规范落地：基础设施层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/player/domain/reward.h"
#include "runtime/storage/mysql/mysql_client_pool.h"
#include "modules/player/ports/player_repository.h"

#include <atomic>

namespace game_server::player {

// 基于 MySQL 的实现：承接对应边界的数据读写与一致性约束。
class MySqlPlayerRepository final : public PlayerRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    explicit MySqlPlayerRepository(common::mysql::MySqlClientPool& mysql_pool);

    [[nodiscard]] std::optional<common::model::PlayerState> LoadPlayerState(std::int64_t player_id) const override;
    [[nodiscard]] BattleEntrySnapshotResult GetBattleEntrySnapshot(std::int64_t player_id) const override;
    [[nodiscard]] PrepareBattleEntryResult PrepareBattleEntry(std::int64_t player_id,
                                                              std::int64_t session_id,
                                                              int energy_cost,
                                                              const std::string& idempotency_key) override;
    [[nodiscard]] CancelBattleEntryResult CancelBattleEntry(std::int64_t player_id,
                                                            std::int64_t session_id,
                                                            int energy_refund,
                                                            const std::string& idempotency_key) override;
    [[nodiscard]] ApplyRewardGrantResult ApplyRewardGrant(std::int64_t player_id,
                                                          std::int64_t grant_id,
                                                          std::int64_t session_id,
                                                          const std::vector<common::model::Reward>& rewards,
                                                          const std::string& idempotency_key) override;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] static std::string ShardSuffix(std::int64_t player_id);
    [[nodiscard]] static std::string ProfileTable(std::int64_t player_id);
    [[nodiscard]] static std::string CurrencyTable(std::int64_t player_id);
    [[nodiscard]] static std::string CurrencyTxnTable(std::int64_t player_id);
    [[nodiscard]] static std::string RoleTable(std::int64_t player_id);
    [[nodiscard]] static std::string StageProgressTable();
    [[nodiscard]] static std::string ItemTxnTable(std::int64_t player_id);
    [[nodiscard]] static std::string PlayerOutboxTable(std::int64_t player_id);
    [[nodiscard]] static std::string CurrencyIdempotencyKey(const std::string& idempotency_key,
                                                            const std::string& currency_type);

    common::mysql::MySqlClientPool& mysql_pool_;
};

}  // namespace game_server::player

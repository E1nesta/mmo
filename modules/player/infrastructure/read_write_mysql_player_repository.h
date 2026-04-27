// 代码规范落地：基础设施层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/player/infrastructure/mysql_player_repository.h"
#include "runtime/storage/mysql/mysql_read_write_client_pool.h"

#include <atomic>

namespace game_server::player {

class ReadWriteMySqlPlayerRepository final : public PlayerRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    explicit ReadWriteMySqlPlayerRepository(common::mysql::MySqlReadWriteClientPool& mysql_pool);

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

    [[nodiscard]] std::uint64_t ReaderHitCount() const;
    [[nodiscard]] std::uint64_t WriterFallbackCount() const;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] std::optional<common::model::PlayerState> LoadPlayerStateFromPool(
        common::mysql::MySqlClientPool& mysql_pool,
        std::int64_t player_id,
        std::string* error_message) const;

    common::mysql::MySqlReadWriteClientPool* mysql_pool_ = nullptr;
    MySqlPlayerRepository writer_repository_;
    mutable std::atomic<std::uint64_t> reader_hit_count_{0};
    mutable std::atomic<std::uint64_t> writer_fallback_count_{0};
};

}  // namespace game_server::player

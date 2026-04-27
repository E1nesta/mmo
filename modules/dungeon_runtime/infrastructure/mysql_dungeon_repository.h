// 代码规范落地：基础设施层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/dungeon_runtime/domain/dungeon_context.h"
#include "modules/dungeon_runtime/domain/reward.h"
#include "modules/dungeon_runtime/domain/stage_config.h"
#include "modules/dungeon_runtime/ports/dungeon_repository.h"
#include "runtime/storage/mysql/mysql_client_pool.h"

#include <atomic>
#include <optional>
#include <string>
#include <vector>

namespace game_server::dungeon_runtime {

class MySqlDungeonRepository final : public DungeonRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    explicit MySqlDungeonRepository(common::mysql::MySqlClientPool& mysql_pool);
    explicit MySqlDungeonRepository(common::mysql::MySqlClient& mysql_client);
    ~MySqlDungeonRepository() override = default;

    [[nodiscard]] std::optional<DungeonContext> FindDungeonBySessionId(std::int64_t session_id) const override;
    [[nodiscard]] std::optional<DungeonEntryOperation> FindDungeonEntryOperationByIdempotencyKey(
        const std::string& idempotency_key) const override;
    [[nodiscard]] bool CreateDungeonEntryOperation(std::int64_t player_id,
                                                  int stage_id,
                                                  const std::string& mode,
                                                  std::int64_t session_id,
                                                  std::int64_t seed,
                                                  const std::string& idempotency_key,
                                                  std::string* error_message = nullptr) override;
    [[nodiscard]] bool MarkDungeonEntryOperationRolledBack(const std::string& idempotency_key,
                                                          std::string* error_message = nullptr) override;
    [[nodiscard]] bool MarkDungeonEntryOperationFailed(const std::string& idempotency_key,
                                                      common::error::ErrorCode error_code,
                                                      const std::string& error_message,
                                                      std::string* storage_error_message = nullptr) override;
    [[nodiscard]] std::optional<DungeonContext> FindActiveDungeonByIdempotencyKey(
        const std::string& idempotency_key) const override;
    [[nodiscard]] std::optional<DungeonContext> FindActiveDungeonByPlayerId(
        std::int64_t player_id) const override;
    [[nodiscard]] EnterDungeonResult CreateDungeonSession(std::int64_t session_id,
                                                        std::int64_t player_id,
                                                        int stage_id,
                                                        const std::string& mode,
                                                        int cost_energy,
                                                        int remain_energy_after,
                                                        const std::vector<DungeonRoleSummary>& role_summaries,
                                                        std::int64_t seed,
                                                        const std::string& idempotency_key,
                                                        const std::string& trace_id) override;
    bool CancelDungeonSession(std::int64_t session_id, std::string* error_message = nullptr) override;
    [[nodiscard]] SettleDungeonResult RecordDungeonSettlement(std::int64_t session_id,
                                                            std::int64_t player_id,
                                                            int stage_id,
                                                            int result_code,
                                                            int star,
                                                            int cost_time_ms,
                                                            std::int64_t client_score,
                                                            std::int64_t reward_grant_id,
                                                            const std::vector<Reward>& rewards,
                                                            const std::string& idempotency_key) override;
    [[nodiscard]] SettleDungeonResult MarkRewardGrantGranted(std::int64_t reward_grant_id) override;
    [[nodiscard]] RewardGrantStatusResult GetRewardGrantStatus(std::int64_t player_id,
                                                               std::int64_t reward_grant_id) const override;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] static std::string EnterOperationTable();
    [[nodiscard]] static std::string ActiveSessionTable();
    [[nodiscard]] static std::string SessionTable(const std::string& month_suffix);
    [[nodiscard]] static std::string TeamSnapshotTable(const std::string& month_suffix);
    [[nodiscard]] static std::string ResultTable(const std::string& month_suffix);
    [[nodiscard]] static std::string RewardGrantTable(const std::string& month_suffix);

    common::mysql::MySqlClientPool* mysql_pool_ = nullptr;
    common::mysql::MySqlClient* mysql_client_ = nullptr;
};

}  // namespace game_server::dungeon_runtime

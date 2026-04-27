// 代码规范落地：端口契约层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/error/error_code.h"

#include "modules/dungeon_runtime/domain/dungeon_context.h"
#include "modules/dungeon_runtime/domain/stage_config.h"
#include "modules/dungeon_runtime/domain/player_snapshot.h"
#include "modules/dungeon_runtime/domain/reward.h"

#include <optional>
#include <string>
#include <vector>

namespace game_server::dungeon_runtime {

enum class DungeonRepositoryError {
    kNone,
    kStorageFailure,
    kStaminaNotEnough,
    kUnfinishedDungeonExists,
    kDungeonAlreadySettled,
    kGrantNotFound,
};

struct EnterDungeonResult {
    bool success = false;
    DungeonRepositoryError error = DungeonRepositoryError::kNone;
    std::string error_message;
    DungeonContext dungeon_context;
};

enum class DungeonEntryOperationStatus {
    kPreparing = 0,
    kActive = 1,
    kRolledBack = 2,
    kCompleted = 3,
    kFailed = 4,
};

struct DungeonEntryOperation {
    std::int64_t player_id = 0;
    int stage_id = 0;
    std::string mode;
    std::int64_t session_id = 0;
    std::int64_t seed = 0;
    int remain_energy_after = 0;
    DungeonEntryOperationStatus status = DungeonEntryOperationStatus::kPreparing;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
};

struct SettleDungeonResult {
    bool success = false;
    DungeonRepositoryError error = DungeonRepositoryError::kNone;
    std::string error_message;
};

struct RewardGrantStatusResult {
    bool success = false;
    int grant_status = 0;
    std::vector<Reward> rewards;
    DungeonRepositoryError error = DungeonRepositoryError::kNone;
    std::string error_message;
};

class DungeonRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    virtual ~DungeonRepository() = default;

    [[nodiscard]] virtual std::optional<DungeonContext> FindDungeonBySessionId(std::int64_t session_id) const = 0;
    [[nodiscard]] virtual std::optional<DungeonEntryOperation> FindDungeonEntryOperationByIdempotencyKey(
        const std::string& idempotency_key) const = 0;
    [[nodiscard]] virtual bool CreateDungeonEntryOperation(std::int64_t player_id,
                                                          int stage_id,
                                                          const std::string& mode,
                                                          std::int64_t session_id,
                                                          std::int64_t seed,
                                                          const std::string& idempotency_key,
                                                          std::string* error_message = nullptr) = 0;
    [[nodiscard]] virtual bool MarkDungeonEntryOperationRolledBack(const std::string& idempotency_key,
                                                                  std::string* error_message = nullptr) = 0;
    [[nodiscard]] virtual bool MarkDungeonEntryOperationFailed(const std::string& idempotency_key,
                                                              common::error::ErrorCode error_code,
                                                              const std::string& error_message,
                                                              std::string* storage_error_message = nullptr) = 0;
    [[nodiscard]] virtual std::optional<DungeonContext> FindActiveDungeonByIdempotencyKey(
        const std::string& idempotency_key) const = 0;
    [[nodiscard]] virtual std::optional<DungeonContext> FindActiveDungeonByPlayerId(
        std::int64_t player_id) const = 0;
    [[nodiscard]] virtual EnterDungeonResult CreateDungeonSession(std::int64_t session_id,
                                                                std::int64_t player_id,
                                                                int stage_id,
                                                                const std::string& mode,
                                                                int cost_energy,
                                                                int remain_energy_after,
                                                                const std::vector<DungeonRoleSummary>& role_summaries,
                                                                std::int64_t seed,
                                                                const std::string& idempotency_key,
                                                                const std::string& trace_id) = 0;
    virtual bool CancelDungeonSession(std::int64_t session_id, std::string* error_message = nullptr) = 0;
    [[nodiscard]] virtual SettleDungeonResult RecordDungeonSettlement(std::int64_t session_id,
                                                                    std::int64_t player_id,
                                                                    int stage_id,
                                                                    int result_code,
                                                                    int star,
                                                                    int cost_time_ms,
                                                                    std::int64_t client_score,
                                                                    std::int64_t reward_grant_id,
                                                                    const std::vector<Reward>& rewards,
                                                                    const std::string& idempotency_key) = 0;
    [[nodiscard]] virtual SettleDungeonResult MarkRewardGrantGranted(std::int64_t reward_grant_id) = 0;
    [[nodiscard]] virtual RewardGrantStatusResult GetRewardGrantStatus(std::int64_t player_id,
                                                                       std::int64_t reward_grant_id) const = 0;
};

}  // namespace game_server::dungeon_runtime

// 代码规范落地：业务编排层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/error/error_code.h"
#include "runtime/foundation/id/id_generator.h"
#include "modules/dungeon_runtime/domain/dungeon_context.h"
#include "modules/dungeon_runtime/domain/reward.h"
#include "modules/dungeon_runtime/domain/player_snapshot.h"
#include "modules/dungeon_runtime/ports/dungeon_context_repository.h"
#include "modules/dungeon_runtime/ports/stage_config_repository.h"
#include "modules/dungeon_runtime/ports/dungeon_repository.h"
#include "modules/dungeon_runtime/ports/player_snapshot_port.h"
#include "modules/dungeon_runtime/ports/player_lock_repository.h"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace game_server::dungeon_runtime {

// 入战用例的应用层请求模型。
struct EnterDungeonRequest {
    std::int64_t player_id = 0;
    std::uint64_t request_id = 0;
    int stage_id = 0;
    std::string mode = "pve";
    int loadout_id = 0;
    std::vector<DungeonUseItem> use_item_list;
};

struct EnterDungeonResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    std::int64_t session_id = 0;
    int remain_stamina = 0;
    std::int64_t seed = 0;
    std::string settle_token;
};

struct SettleDungeonRequest {
    std::int64_t player_id = 0;
    std::int64_t session_id = 0;
    int stage_id = 0;
    int star = 0;
    int result_code = 1;
    std::int64_t client_score = 0;
    std::string settle_token;
    DungeonBattleStats battle_stats;
};

struct SettleDungeonResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    std::int64_t reward_grant_id = 0;
    int grant_status = 0;
    std::vector<Reward> reward_preview;
    std::vector<DungeonUseItem> use_item_list;
};

struct SettlementGrantStatusResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    std::int64_t reward_grant_id = 0;
    int grant_status = 0;
    std::vector<Reward> rewards;
};

struct ActiveDungeonResponse {
    bool success = false;
    common::error::ErrorCode error_code = common::error::ErrorCode::kOk;
    std::string error_message;
    bool found = false;
    std::int64_t session_id = 0;
    int stage_id = 0;
    std::string mode;
    int remain_stamina = 0;
    std::int64_t seed = 0;
    std::string settle_token;
};

// 应用服务负责关卡规则、玩家锁与持久化边界的协同编排。
class DungeonRuntimeService {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    DungeonRuntimeService(PlayerLockRepository& player_lock_repository,
                   PlayerSnapshotPort& player_snapshot_port,
                   StageConfigRepository& stage_config_repository,
                   DungeonRepository& dungeon_repository,
                   DungeonContextRepository& dungeon_context_repository,
                   std::uint16_t id_generator_node_id = 1,
                   std::string settle_token_secret = "dungeon-settle-token");

    [[nodiscard]] EnterDungeonResponse EnterDungeon(const EnterDungeonRequest& request, const std::string& trace_id);
    [[nodiscard]] SettleDungeonResponse SettleDungeon(const SettleDungeonRequest& request, const std::string& trace_id);
    [[nodiscard]] ActiveDungeonResponse GetActiveDungeon(std::int64_t player_id) const;
    [[nodiscard]] SettlementGrantStatusResponse GetSettlementGrantStatus(std::int64_t player_id,
                                                                 std::int64_t reward_grant_id) const;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] std::optional<StageConfig> LoadStageConfig(int stage_id) const;
    [[nodiscard]] GetDungeonEntrySnapshotPortResponse LoadPlayerSnapshot(std::int64_t player_id) const;
    [[nodiscard]] std::optional<DungeonContext> LoadDungeonContext(std::int64_t session_id) const;
    [[nodiscard]] bool RollbackDungeonEntry(std::int64_t player_id,
                                           std::int64_t session_id,
                                           int energy_refund,
                                           int expected_stamina_after_rollback,
                                           const std::string& entry_idempotency_key,
                                           std::string* error_message) const;
    [[nodiscard]] std::optional<EnterDungeonResponse> ValidateEnterRequirements(
        const PlayerSnapshot& player_snapshot,
        const StageConfig& stage_config) const;
    [[nodiscard]] std::optional<SettleDungeonResponse> ValidateSettleInput(
        const SettleDungeonRequest& request,
        const StageConfig& stage_config) const;
    [[nodiscard]] std::optional<SettleDungeonResponse> ValidateDungeonContext(
        const SettleDungeonRequest& request,
        const DungeonContext& dungeon_context) const;
    [[nodiscard]] std::string BuildSettleToken(std::int64_t player_id,
                                               std::int64_t session_id,
                                               int stage_id,
                                               std::int64_t seed) const;
    [[nodiscard]] common::error::ErrorCode MapEnterStorageError(DungeonRepositoryError error) const;
    [[nodiscard]] common::error::ErrorCode MapSettleStorageError(DungeonRepositoryError error) const;
    [[nodiscard]] bool AcquirePlayerLock(std::int64_t player_id);
    void ReleasePlayerLock(std::int64_t player_id);

    // 跨边界依赖：锁、快照、关卡配置、正式存储与运行态上下文仓储。
    PlayerLockRepository& player_lock_repository_;
    PlayerSnapshotPort& player_snapshot_port_;
    StageConfigRepository& stage_config_repository_;
    DungeonRepository& dungeon_repository_;
    DungeonContextRepository& dungeon_context_repository_;

    // 本地运行态：会话 ID 生成器与结算令牌签名密钥。
    common::id::IdGenerator id_generator_;
    std::string settle_token_secret_;
};

}  // namespace game_server::dungeon_runtime

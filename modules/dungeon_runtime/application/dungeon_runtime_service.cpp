// 代码规范落地：业务编排层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/dungeon_runtime/application/dungeon_runtime_service.h"

#include "runtime/foundation/log/logger.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <chrono>
#include <functional>
#include <optional>
#include <sstream>
#include <string_view>

namespace game_server::dungeon_runtime {

namespace {

constexpr int kDungeonResultLose = 0;
constexpr int kDungeonResultWin = 1;
constexpr int kDungeonEntryRollbackAttempts = 3;

EnterDungeonResponse BuildEnterError(common::error::ErrorCode error_code, std::string error_message) {
    return {false, error_code, std::move(error_message), 0, 0, 0, ""};
}

SettleDungeonResponse BuildSettleError(common::error::ErrorCode error_code, std::string error_message) {
    return {false, error_code, std::move(error_message), 0, 0, {}, {}};
}

SettlementGrantStatusResponse BuildGrantStatusError(common::error::ErrorCode error_code, std::string error_message) {
    return {false, error_code, std::move(error_message), 0, 0, {}};
}

ActiveDungeonResponse BuildActiveDungeonError(common::error::ErrorCode error_code, std::string error_message) {
    return {false, error_code, std::move(error_message), false, 0, 0, "", 0, 0, ""};
}

struct ScopedPlayerLock {
    std::function<void()> release;
    bool acquired = false;

    ~ScopedPlayerLock() {
        if (acquired && release) {
            release();
        }
    }
};

std::string EntryIdempotencyKey(std::int64_t player_id, std::uint64_t request_id) {
    return "dungeon-enter:" + std::to_string(player_id) + ":" + std::to_string(request_id);
}

std::string SettleIdempotencyKey(std::int64_t player_id, std::int64_t session_id) {
    return "dungeon-settle:" + std::to_string(player_id) + ":" + std::to_string(session_id);
}

std::string ToHex(const unsigned char* data, std::size_t size) {
    static constexpr char kHexDigits[] = "0123456789abcdef";

    std::string output;
    output.reserve(size * 2);
    for (std::size_t index = 0; index < size; ++index) {
        output.push_back(kHexDigits[(data[index] >> 4U) & 0x0FU]);
        output.push_back(kHexDigits[data[index] & 0x0FU]);
    }
    return output;
}

std::optional<std::string> ComputeHmacSha256Hex(std::string_view key, std::string_view payload) {
    unsigned int digest_length = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];
    if (HMAC(EVP_sha256(),
             key.data(),
             static_cast<int>(key.size()),
             reinterpret_cast<const unsigned char*>(payload.data()),
             payload.size(),
             digest,
             &digest_length) == nullptr) {
        return std::nullopt;
    }
    return ToHex(digest, digest_length);
}

bool TimingSafeEqual(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    return CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}

}  // namespace

DungeonRuntimeService::DungeonRuntimeService(PlayerLockRepository& player_lock_repository,
                               PlayerSnapshotPort& player_snapshot_port,
                               StageConfigRepository& stage_config_repository,
                               DungeonRepository& dungeon_repository,
                               DungeonContextRepository& dungeon_context_repository,
                               std::uint16_t id_generator_node_id,
                               std::string settle_token_secret)
    : player_lock_repository_(player_lock_repository),
      player_snapshot_port_(player_snapshot_port),
      stage_config_repository_(stage_config_repository),
      dungeon_repository_(dungeon_repository),
      dungeon_context_repository_(dungeon_context_repository),
      id_generator_(id_generator_node_id),
      settle_token_secret_(std::move(settle_token_secret)) {}

// 状态推进：`EnterDungeon` 执行写链或补偿并收敛状态变化。
EnterDungeonResponse DungeonRuntimeService::EnterDungeon(const EnterDungeonRequest& request, const std::string& trace_id) {
    // 前置校验阶段：先校验请求上下文和关卡配置。
    if (request.request_id == 0) {
        return BuildEnterError(common::error::ErrorCode::kRequestContextInvalid, "request_id is invalid");
    }
    const auto config = LoadStageConfig(request.stage_id);
    if (!config.has_value()) {
        return BuildEnterError(common::error::ErrorCode::kStageNotFound, "stage config not found");
    }

    // 并发控制阶段：同一玩家同一时刻只允许一个入战流程推进。
    ScopedPlayerLock player_lock{
        [this, player_id = request.player_id] { ReleasePlayerLock(player_id); },
        AcquirePlayerLock(request.player_id)};
    if (!player_lock.acquired) {
        return BuildEnterError(common::error::ErrorCode::kPlayerBusy, "player is busy");
    }

    const auto entry_idempotency_key = EntryIdempotencyKey(request.player_id, request.request_id);
    if (const auto replay = dungeon_repository_.FindActiveDungeonByIdempotencyKey(entry_idempotency_key);
        replay.has_value()) {
        return {true,
                common::error::ErrorCode::kOk,
                "",
                replay->session_id,
                replay->remain_energy_after,
                replay->seed,
                BuildSettleToken(replay->player_id, replay->session_id, replay->stage_id, replay->seed)};
    }

    // 输入准备阶段：恢复或创建本次入战操作上下文。
    PlayerSnapshot snapshot_data;
    std::int64_t session_id = 0;
    std::int64_t seed = 0;
    if (const auto operation = dungeon_repository_.FindDungeonEntryOperationByIdempotencyKey(entry_idempotency_key);
        operation.has_value()) {
        if (operation->player_id != request.player_id || operation->stage_id != request.stage_id || operation->mode != request.mode) {
            return BuildEnterError(common::error::ErrorCode::kBattleMismatch, "dungeon entry request mismatch");
        }
        if (operation->status == DungeonEntryOperationStatus::kActive) {
            return {true,
                    common::error::ErrorCode::kOk,
                    "",
                    operation->session_id,
                    operation->remain_energy_after,
                    operation->seed,
                    BuildSettleToken(operation->player_id,
                                     operation->session_id,
                                     operation->stage_id,
                                     operation->seed)};
        }
        if (operation->status == DungeonEntryOperationStatus::kRolledBack) {
            return BuildEnterError(common::error::ErrorCode::kStorageError, "dungeon entry operation already rolled back");
        }
        if (operation->status == DungeonEntryOperationStatus::kCompleted) {
            return BuildEnterError(common::error::ErrorCode::kBattleAlreadySettled,
                                   "dungeon entry operation already completed");
        }
        if (operation->status == DungeonEntryOperationStatus::kFailed) {
            return BuildEnterError(operation->error_code, operation->error_message);
        }
        const auto snapshot = LoadPlayerSnapshot(request.player_id);
        if (!snapshot.success) {
            return BuildEnterError(snapshot.error_code, snapshot.error_message);
        }
        if (!snapshot.found) {
            return BuildEnterError(common::error::ErrorCode::kPlayerNotFound, "player not found");
        }
        snapshot_data = snapshot.snapshot;
        session_id = operation->session_id;
        seed = operation->seed;
    } else {
        if (const auto unfinished = dungeon_repository_.FindActiveDungeonByPlayerId(request.player_id);
            unfinished.has_value()) {
            return BuildEnterError(common::error::ErrorCode::kPlayerBusy, "unfinished dungeon exists");
        }

        const auto snapshot = LoadPlayerSnapshot(request.player_id);
        if (!snapshot.success) {
            return BuildEnterError(snapshot.error_code, snapshot.error_message);
        }
        if (!snapshot.found) {
            return BuildEnterError(common::error::ErrorCode::kPlayerNotFound, "player not found");
        }
        if (const auto validation = ValidateEnterRequirements(snapshot.snapshot, *config); validation.has_value()) {
            return *validation;
        }
        snapshot_data = snapshot.snapshot;

        session_id = id_generator_.Next();
        seed = id_generator_.Next();
        std::string create_operation_error;
        if (!dungeon_repository_.CreateDungeonEntryOperation(
                request.player_id,
                request.stage_id,
                request.mode,
                session_id,
                seed,
                entry_idempotency_key,
                &create_operation_error)) {
            common::log::Logger::Instance().Log(
                common::log::LogLevel::kWarn,
                "dungeon enter audit: failed to create entry operation player_id=" + std::to_string(request.player_id) +
                    " stage_id=" + std::to_string(request.stage_id) + " error=" + create_operation_error);
            return BuildEnterError(common::error::ErrorCode::kStorageError,
                                   "failed to create dungeon entry operation: " + create_operation_error);
        }
    }

    // 正式写入阶段：先走玩家写链预扣体力，再创建副本正式会话。
    const auto prepare = player_snapshot_port_.PrepareDungeonEntry(
        request.player_id, session_id, config->cost_stamina, entry_idempotency_key);
    if (!prepare.success) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon enter audit: prepare failed player_id=" + std::to_string(request.player_id) +
                " session_id=" + std::to_string(session_id) + " error=" + prepare.error_message);
        if (prepare.error_code != common::error::ErrorCode::kStorageError &&
            prepare.error_code != common::error::ErrorCode::kServiceUnavailable) {
            std::string mark_failed_error;
            if (!dungeon_repository_.MarkDungeonEntryOperationFailed(
                    entry_idempotency_key, prepare.error_code, prepare.error_message, &mark_failed_error)) {
                common::log::Logger::Instance().Log(
                    common::log::LogLevel::kError,
                    "dungeon enter compensation failed: mark operation failed player_id=" +
                        std::to_string(request.player_id) + " session_id=" + std::to_string(session_id) +
                        " error=" + mark_failed_error);
                return BuildEnterError(common::error::ErrorCode::kStorageError,
                                       "dungeon entry prepare failed and failure finalize failed: " +
                                           mark_failed_error);
            }
        }
        return BuildEnterError(prepare.error_code, prepare.error_message);
    }

    // 正式态推进：创建副本会话记录，失败时进入补偿回滚链路。
    const auto create_result = dungeon_repository_.CreateDungeonSession(session_id,
                                                                       request.player_id,
                                                                       request.stage_id,
                                                                       request.mode,
                                                                       config->cost_stamina,
                                                                       prepare.remain_energy,
                                                                       snapshot_data.role_summaries,
                                                                       seed,
                                                                       entry_idempotency_key,
                                                                       trace_id);
    if (!create_result.success) {
        std::string rollback_error_message;
        if (!RollbackDungeonEntry(request.player_id,
                                 session_id,
                                 config->cost_stamina,
                                 prepare.remain_energy + config->cost_stamina,
                                 entry_idempotency_key,
                                 &rollback_error_message)) {
            common::log::Logger::Instance().Log(
                common::log::LogLevel::kError,
                "dungeon enter compensation failed: rollback stamina failed player_id=" +
                    std::to_string(request.player_id) + " session_id=" + std::to_string(session_id) +
                    " error=" + rollback_error_message);
            return BuildEnterError(common::error::ErrorCode::kStorageError,
                                   "dungeon session creation failed and stamina rollback failed: " +
                                       rollback_error_message);
        }
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon enter compensation applied: session creation failed but rollback succeeded player_id=" +
                std::to_string(request.player_id) + " session_id=" + std::to_string(session_id));
        std::string mark_rolled_back_error;
        if (!dungeon_repository_.MarkDungeonEntryOperationRolledBack(entry_idempotency_key, &mark_rolled_back_error)) {
            common::log::Logger::Instance().Log(
                common::log::LogLevel::kError,
                "dungeon enter compensation failed: mark rolled back failed player_id=" +
                    std::to_string(request.player_id) + " session_id=" + std::to_string(session_id) +
                    " error=" + mark_rolled_back_error);
            return BuildEnterError(common::error::ErrorCode::kStorageError,
                                   "dungeon session creation failed and rollback finalize failed: " +
                                       mark_rolled_back_error);
        }
        return BuildEnterError(MapEnterStorageError(create_result.error), create_result.error_message);
    }

    // 运行态收敛：写入运行时上下文，失败时回滚正式态并回收预扣。
    auto persisted_context = create_result.dungeon_context;
    persisted_context.loadout_id = request.loadout_id;
    persisted_context.use_item_list = request.use_item_list;
    if (!dungeon_context_repository_.Save(persisted_context)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon enter audit: runtime context save failed player_id=" + std::to_string(request.player_id) +
                " session_id=" + std::to_string(session_id));

        std::string cancel_session_error;
        if (!dungeon_repository_.CancelDungeonSession(session_id, &cancel_session_error)) {
            common::log::Logger::Instance().Log(
                common::log::LogLevel::kError,
                "dungeon enter compensation failed: cancel session failed after runtime context save failure player_id=" +
                    std::to_string(request.player_id) + " session_id=" + std::to_string(session_id) +
                    " error=" + cancel_session_error);
            return BuildEnterError(common::error::ErrorCode::kStorageError,
                                   "failed to persist dungeon runtime context and cancel dungeon session: " +
                                       cancel_session_error);
        }

        std::string rollback_error_message;
        if (!RollbackDungeonEntry(request.player_id,
                                 session_id,
                                 config->cost_stamina,
                                 prepare.remain_energy + config->cost_stamina,
                                 entry_idempotency_key,
                                 &rollback_error_message)) {
            common::log::Logger::Instance().Log(
                common::log::LogLevel::kError,
                "dungeon enter compensation failed: rollback stamina failed after runtime context save failure player_id=" +
                    std::to_string(request.player_id) + " session_id=" + std::to_string(session_id) +
                    " error=" + rollback_error_message);

            std::string mark_failed_error;
            if (!dungeon_repository_.MarkDungeonEntryOperationFailed(
                    entry_idempotency_key,
                    common::error::ErrorCode::kStorageError,
                    "runtime context save failed after session creation and stamina rollback failed: " +
                        rollback_error_message,
                    &mark_failed_error)) {
                common::log::Logger::Instance().Log(
                    common::log::LogLevel::kError,
                    "dungeon enter compensation failed: mark operation failed after runtime context save failure player_id=" +
                        std::to_string(request.player_id) + " session_id=" + std::to_string(session_id) +
                        " error=" + mark_failed_error);
                return BuildEnterError(common::error::ErrorCode::kStorageError,
                                       "runtime context save failed and failure finalize failed: " +
                                           mark_failed_error);
            }

            return BuildEnterError(common::error::ErrorCode::kStorageError,
                                   "failed to persist dungeon runtime context and rollback stamina: " +
                                       rollback_error_message);
        }

        std::string mark_rolled_back_error;
        if (!dungeon_repository_.MarkDungeonEntryOperationRolledBack(entry_idempotency_key, &mark_rolled_back_error)) {
            common::log::Logger::Instance().Log(
                common::log::LogLevel::kError,
                "dungeon enter compensation failed: mark rolled back failed after runtime context save failure player_id=" +
                    std::to_string(request.player_id) + " session_id=" + std::to_string(session_id) +
                    " error=" + mark_rolled_back_error);
            return BuildEnterError(common::error::ErrorCode::kStorageError,
                                   "runtime context save failed and rollback finalize failed: " +
                                       mark_rolled_back_error);
        }

        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon enter compensation applied: runtime context save failed and session rolled back player_id=" +
                std::to_string(request.player_id) + " session_id=" + std::to_string(session_id));
        return BuildEnterError(common::error::ErrorCode::kStorageError, "failed to persist dungeon runtime context");
    }

    // 返回阶段：输出入战成功结果与后续结算令牌。
    return {true,
            common::error::ErrorCode::kOk,
            "",
            session_id,
            prepare.remain_energy,
            seed,
            BuildSettleToken(request.player_id, session_id, request.stage_id, seed)};
}

// 状态推进：`SettleDungeon` 执行写链或补偿并收敛状态变化。
SettleDungeonResponse DungeonRuntimeService::SettleDungeon(const SettleDungeonRequest& request, const std::string& trace_id) {
    (void)trace_id;

    // 前置校验阶段：校验关卡配置与结算输入。
    const auto config = LoadStageConfig(request.stage_id);
    if (!config.has_value()) {
        return BuildSettleError(common::error::ErrorCode::kStageNotFound, "stage config not found");
    }
    if (const auto validation = ValidateSettleInput(request, *config); validation.has_value()) {
        return *validation;
    }

    // 并发控制阶段：串行化同一玩家结算请求。
    ScopedPlayerLock player_lock{
        [this, player_id = request.player_id] { ReleasePlayerLock(player_id); },
        AcquirePlayerLock(request.player_id)};
    if (!player_lock.acquired) {
        return BuildSettleError(common::error::ErrorCode::kPlayerBusy, "player is busy");
    }

    const auto dungeon_context = LoadDungeonContext(request.session_id);
    if (!dungeon_context.has_value()) {
        return BuildSettleError(common::error::ErrorCode::kBattleNotFound, "dungeon not found");
    }
    if (const auto validation = ValidateDungeonContext(request, *dungeon_context); validation.has_value()) {
        return *validation;
    }

    // 幂等重放阶段：已结算会话优先回放既有结果，必要时补偿发奖状态。
    if (dungeon_context->settled) {
        auto status = dungeon_repository_.GetRewardGrantStatus(request.player_id, dungeon_context->reward_grant_id);
        if (!status.success) {
            return BuildSettleError(common::error::ErrorCode::kBattleAlreadySettled, "dungeon already settled");
        }
        if (status.grant_status == 0) {
            const auto settle_idempotency_key = SettleIdempotencyKey(request.player_id, request.session_id);
            const auto apply_reward = player_snapshot_port_.ApplyRewardGrant(
                request.player_id, dungeon_context->reward_grant_id, request.session_id, status.rewards, settle_idempotency_key);
            if (!apply_reward.success) {
                common::log::Logger::Instance().Log(
                    common::log::LogLevel::kError,
                    "dungeon settle compensation failed: replay reward apply failed player_id=" +
                        std::to_string(request.player_id) + " session_id=" + std::to_string(request.session_id) +
                        " grant_id=" + std::to_string(dungeon_context->reward_grant_id) +
                        " error=" + apply_reward.error_message);
                return BuildSettleError(apply_reward.error_code, apply_reward.error_message);
            }

            const auto mark_granted = dungeon_repository_.MarkRewardGrantGranted(dungeon_context->reward_grant_id);
            if (!mark_granted.success) {
                common::log::Logger::Instance().Log(
                    common::log::LogLevel::kError,
                    "dungeon settle compensation failed: replay mark granted failed player_id=" +
                        std::to_string(request.player_id) + " session_id=" + std::to_string(request.session_id) +
                        " grant_id=" + std::to_string(dungeon_context->reward_grant_id) +
                        " error=" + mark_granted.error_message);
                return BuildSettleError(MapSettleStorageError(mark_granted.error), mark_granted.error_message);
            }

            if (!dungeon_context_repository_.Delete(request.session_id)) {
                common::log::Logger::Instance().Log(
                    common::log::LogLevel::kWarn,
                    "dungeon settle audit: replay reward granted but runtime context delete failed player_id=" +
                        std::to_string(request.player_id) + " session_id=" + std::to_string(request.session_id));
            }
            status.grant_status = 1;
        }
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kInfo,
            "dungeon settle replay: player_id=" + std::to_string(request.player_id) + " session_id=" +
                std::to_string(request.session_id) + " grant_id=" +
                std::to_string(dungeon_context->reward_grant_id) + " grant_status=" +
                std::to_string(status.grant_status));
        return {true,
                common::error::ErrorCode::kOk,
                "",
                dungeon_context->reward_grant_id,
                status.grant_status,
                status.rewards,
                dungeon_context->use_item_list};
    }

    // 运行态同步阶段：先落盘本次战斗统计，再推进正式结算状态。
    auto runtime_context = *dungeon_context;
    runtime_context.battle_stats = request.battle_stats;
    if (!dungeon_context_repository_.Save(runtime_context)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon settle audit: runtime context save failed before settlement player_id=" +
                std::to_string(request.player_id) + " session_id=" + std::to_string(request.session_id));
        return BuildSettleError(common::error::ErrorCode::kStorageError,
                                "failed to persist dungeon runtime context before settlement");
    }

    // 当前 MVP 在胜利时只发放固定通关奖励。
    // 客户端上报星级不直接铸造高价值货币。
    std::vector<Reward> rewards;
    if (request.result_code == kDungeonResultWin) {
        rewards.push_back({"gold", config->normal_gold_reward});
    }

    // 正式结算阶段：记录战斗结算结果并持久化奖励草案。
    const auto reward_grant_id = request.session_id;
    const auto settle_idempotency_key = SettleIdempotencyKey(request.player_id, request.session_id);
    const auto settle_result = dungeon_repository_.RecordDungeonSettlement(request.session_id,
                                                                          request.player_id,
                                                                          request.stage_id,
                                                                          request.result_code,
                                                                          request.star,
                                                                          request.battle_stats.pass_time_seconds * 1000,
                                                                          request.client_score,
                                                                          reward_grant_id,
                                                                          rewards,
                                                                          settle_idempotency_key);
    if (!settle_result.success) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon settle audit: record settlement failed player_id=" + std::to_string(request.player_id) +
                " session_id=" + std::to_string(request.session_id) + " error=" + settle_result.error_message);
        return BuildSettleError(MapSettleStorageError(settle_result.error), settle_result.error_message);
    }

    // 运行态收敛：将会话标记为已结算，供重试路径幂等回放。
    runtime_context.settled = true;
    runtime_context.reward_grant_id = reward_grant_id;
    runtime_context.grant_status = 0;
    runtime_context.rewards = rewards;
    if (!dungeon_context_repository_.Save(runtime_context)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon settle audit: runtime settled context save failed player_id=" + std::to_string(request.player_id) +
                " session_id=" + std::to_string(request.session_id) + " grant_id=" + std::to_string(reward_grant_id));
        if (!dungeon_context_repository_.Delete(request.session_id)) {
            common::log::Logger::Instance().Log(
                common::log::LogLevel::kWarn,
                "dungeon settle compensation failed: stale runtime context delete failed player_id=" +
                    std::to_string(request.player_id) + " session_id=" + std::to_string(request.session_id));
        }
        return BuildSettleError(common::error::ErrorCode::kStorageError,
                                "failed to persist settled dungeon runtime context");
    }

    // 发奖阶段：调用玩家写链落正式奖励状态。
    const auto apply_reward = player_snapshot_port_.ApplyRewardGrant(
        request.player_id, reward_grant_id, request.session_id, rewards, settle_idempotency_key);
    if (!apply_reward.success) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kError,
            "dungeon settle compensation failed: reward apply failed player_id=" + std::to_string(request.player_id) +
                " session_id=" + std::to_string(request.session_id) + " grant_id=" +
                std::to_string(reward_grant_id) + " error=" + apply_reward.error_message);
        return BuildSettleError(apply_reward.error_code, apply_reward.error_message);
    }

    // 正式态收敛：将奖励状态推进到 granted。
    const auto mark_granted = dungeon_repository_.MarkRewardGrantGranted(reward_grant_id);
    if (!mark_granted.success) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kError,
            "dungeon settle compensation failed: mark granted failed player_id=" +
                std::to_string(request.player_id) + " session_id=" + std::to_string(request.session_id) +
                " grant_id=" + std::to_string(reward_grant_id) + " error=" + mark_granted.error_message);
        return BuildSettleError(MapSettleStorageError(mark_granted.error), mark_granted.error_message);
    }

    // 运行态清理：奖励完成后删除临时上下文。
    if (!dungeon_context_repository_.Delete(request.session_id)) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "dungeon settle audit: reward granted but runtime context delete failed player_id=" +
                std::to_string(request.player_id) + " session_id=" + std::to_string(request.session_id));
    }
    return {true, common::error::ErrorCode::kOk, "", reward_grant_id, 1, rewards, runtime_context.use_item_list};
}

// 状态读取：`GetActiveDungeon` 负责加载上下文并返回稳定结果。
ActiveDungeonResponse DungeonRuntimeService::GetActiveDungeon(std::int64_t player_id) const {
    // 前置校验：玩家标识非法时直接返回错误。
    if (player_id <= 0) {
        return BuildActiveDungeonError(common::error::ErrorCode::kRequestContextInvalid, "player_id is invalid");
    }

    // 读取阶段：优先从正式副本状态中定位活跃会话。
    const auto dungeon_context = dungeon_repository_.FindActiveDungeonByPlayerId(player_id);
    if (!dungeon_context.has_value()) {
        return {true, common::error::ErrorCode::kOk, "", false, 0, 0, "", 0, 0, ""};
    }

    return {true,
            common::error::ErrorCode::kOk,
            "",
            true,
            dungeon_context->session_id,
            dungeon_context->stage_id,
            dungeon_context->mode,
            dungeon_context->remain_energy_after,
            dungeon_context->seed,
            BuildSettleToken(
                dungeon_context->player_id, dungeon_context->session_id, dungeon_context->stage_id, dungeon_context->seed)};
}

// 状态读取：`GetSettlementGrantStatus` 负责加载上下文并返回稳定结果。
SettlementGrantStatusResponse DungeonRuntimeService::GetSettlementGrantStatus(std::int64_t player_id,
                                                              std::int64_t reward_grant_id) const {
    // 读取阶段：查询奖励发放状态，支撑弱网重试回放。
    const auto result = dungeon_repository_.GetRewardGrantStatus(player_id, reward_grant_id);
    if (!result.success) {
        return BuildGrantStatusError(common::error::ErrorCode::kBattleNotFound, result.error_message);
    }
    return {true, common::error::ErrorCode::kOk, "", reward_grant_id, result.grant_status, result.rewards};
}

std::optional<StageConfig> DungeonRuntimeService::LoadStageConfig(int stage_id) const {
    return stage_config_repository_.FindByStageId(stage_id);
}

GetDungeonEntrySnapshotPortResponse DungeonRuntimeService::LoadPlayerSnapshot(std::int64_t player_id) const {
    return player_snapshot_port_.GetDungeonEntrySnapshot(player_id);
}

// 状态读取：`LoadDungeonContext` 负责加载上下文并返回稳定结果。
std::optional<DungeonContext> DungeonRuntimeService::LoadDungeonContext(std::int64_t session_id) const {
    if (auto dungeon_context = dungeon_context_repository_.FindBySessionId(session_id); dungeon_context.has_value()) {
        return dungeon_context;
    }
    return dungeon_repository_.FindDungeonBySessionId(session_id);
}

// 状态推进：`RollbackDungeonEntry` 执行写链或补偿并收敛状态变化。
bool DungeonRuntimeService::RollbackDungeonEntry(std::int64_t player_id,
                                        std::int64_t session_id,
                                        int energy_refund,
                                        int expected_stamina_after_rollback,
                                        const std::string& entry_idempotency_key,
                                        std::string* error_message) const {
    // 补偿阶段：有限次重试回滚预扣，最终以快照状态作为兜底判定。
    std::string last_error_message;
    for (int attempt = 0; attempt < kDungeonEntryRollbackAttempts; ++attempt) {
        const auto cancel_result = player_snapshot_port_.CancelDungeonEntry(
            player_id, session_id, energy_refund, "dungeon-cancel:" + entry_idempotency_key);
        if (cancel_result.success) {
            return true;
        }
        last_error_message = cancel_result.error_message;

        const auto snapshot = LoadPlayerSnapshot(player_id);
        if (snapshot.success && snapshot.found && snapshot.snapshot.stamina == expected_stamina_after_rollback) {
            return true;
        }
    }

    if (error_message != nullptr) {
        *error_message = last_error_message.empty() ? "rollback attempts exhausted" : last_error_message;
    }
    return false;
}

// 输入校验：`ValidateEnterRequirements` 校验关键约束并在失败时快速返回。
std::optional<EnterDungeonResponse> DungeonRuntimeService::ValidateEnterRequirements(const PlayerSnapshot& player_snapshot,
                                                                             const StageConfig& stage_config) const {
    if (player_snapshot.level < stage_config.required_level) {
        return BuildEnterError(common::error::ErrorCode::kStageLocked, "player level not enough");
    }
    if (player_snapshot.stamina < stage_config.cost_stamina) {
        return BuildEnterError(common::error::ErrorCode::kStaminaNotEnough, "stamina not enough");
    }
    return std::nullopt;
}

// 输入校验：`ValidateSettleInput` 校验关键约束并在失败时快速返回。
std::optional<SettleDungeonResponse> DungeonRuntimeService::ValidateSettleInput(const SettleDungeonRequest& request,
                                                                        const StageConfig& stage_config) const {
    if (request.star < 0 || request.star > stage_config.max_star) {
        return BuildSettleError(common::error::ErrorCode::kInvalidStar, "star is out of range");
    }
    if (request.result_code != kDungeonResultLose && request.result_code != kDungeonResultWin) {
        return BuildSettleError(common::error::ErrorCode::kBattleMismatch, "result_code is invalid");
    }
    if (request.result_code == kDungeonResultWin) {
        if (request.star <= 0) {
            return BuildSettleError(common::error::ErrorCode::kBattleMismatch, "win result requires star > 0");
        }
        if (request.client_score <= 0) {
            return BuildSettleError(common::error::ErrorCode::kBattleMismatch, "win result requires positive client_score");
        }
    } else {
        if (request.star != 0) {
            return BuildSettleError(common::error::ErrorCode::kBattleMismatch, "lose result requires star = 0");
        }
        if (request.client_score != 0) {
            return BuildSettleError(common::error::ErrorCode::kBattleMismatch, "lose result requires client_score = 0");
        }
    }
    if (request.settle_token.empty()) {
        return BuildSettleError(common::error::ErrorCode::kBattleMismatch, "settle token is missing");
    }
    return std::nullopt;
}

// 输入校验：`ValidateDungeonContext` 校验关键约束并在失败时快速返回。
std::optional<SettleDungeonResponse> DungeonRuntimeService::ValidateDungeonContext(const SettleDungeonRequest& request,
                                                                          const DungeonContext& dungeon_context) const {
    if (dungeon_context.player_id != request.player_id || dungeon_context.stage_id != request.stage_id) {
        return BuildSettleError(common::error::ErrorCode::kBattleMismatch, "dungeon context mismatch");
    }
    const auto expected_token =
        BuildSettleToken(dungeon_context.player_id, dungeon_context.session_id, dungeon_context.stage_id, dungeon_context.seed);
    if (expected_token.empty() || !TimingSafeEqual(request.settle_token, expected_token)) {
        return BuildSettleError(common::error::ErrorCode::kBattleMismatch, "settle token mismatch");
    }
    return std::nullopt;
}

std::string DungeonRuntimeService::BuildSettleToken(std::int64_t player_id,
                                            std::int64_t session_id,
                                            int stage_id,
                                            std::int64_t seed) const {
    // 令牌阶段：使用稳定字段生成 HMAC，保证结算请求不可伪造。
    std::ostringstream payload;
    payload << player_id << ':' << session_id << ':' << stage_id << ':' << seed;
    const auto token = ComputeHmacSha256Hex(settle_token_secret_, payload.str());
    if (!token.has_value()) {
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kError,
            "dungeon settle audit: failed to compute settle token player_id=" + std::to_string(player_id) +
                " session_id=" + std::to_string(session_id));
        return "";
    }
    return *token;
}

// 输入校验：`MapEnterStorageError` 校验关键约束并在失败时快速返回。
common::error::ErrorCode DungeonRuntimeService::MapEnterStorageError(DungeonRepositoryError error) const {
    if (error == DungeonRepositoryError::kUnfinishedDungeonExists) {
        return common::error::ErrorCode::kPlayerBusy;
    }
    return common::error::ErrorCode::kStorageError;
}

// 输入校验：`MapSettleStorageError` 校验关键约束并在失败时快速返回。
common::error::ErrorCode DungeonRuntimeService::MapSettleStorageError(DungeonRepositoryError error) const {
    if (error == DungeonRepositoryError::kDungeonAlreadySettled) {
        return common::error::ErrorCode::kBattleAlreadySettled;
    }
    if (error == DungeonRepositoryError::kGrantNotFound) {
        return common::error::ErrorCode::kBattleNotFound;
    }
    return common::error::ErrorCode::kStorageError;
}

bool DungeonRuntimeService::AcquirePlayerLock(std::int64_t player_id) {
    // 并发控制：同一玩家请求在跨链路重试时保持串行。
    return player_lock_repository_.Acquire(player_id);
}

void DungeonRuntimeService::ReleasePlayerLock(std::int64_t player_id) {
    // 释放阶段：在请求结束后及时归还玩家锁。
    player_lock_repository_.Release(player_id);
}

}  // namespace game_server::dungeon_runtime

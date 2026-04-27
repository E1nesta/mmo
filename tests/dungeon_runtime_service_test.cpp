#include "modules/dungeon_runtime/application/dungeon_runtime_service.h"
#include "modules/dungeon_runtime/infrastructure/in_memory_stage_config_repository.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

using game_server::dungeon_runtime::DungeonContext;
using game_server::dungeon_runtime::Reward;

class FakePlayerSnapshotPort final : public game_server::dungeon_runtime::PlayerSnapshotPort {
public:
    struct PreparedEntryRecord {
        std::int64_t session_id = 0;
        game_server::dungeon_runtime::PrepareDungeonEntryPortResponse response;
    };

    explicit FakePlayerSnapshotPort(game_server::dungeon_runtime::PlayerSnapshot snapshot) {
        snapshots_.emplace(snapshot.player_id, std::move(snapshot));
    }

    game_server::dungeon_runtime::GetDungeonEntrySnapshotPortResponse GetDungeonEntrySnapshot(std::int64_t player_id) const override {
        if (!snapshot_should_succeed_) {
            return {false, false, snapshot_error_code_, snapshot_error_message_, {}};
        }
        const auto it = snapshots_.find(player_id);
        if (it == snapshots_.end()) {
            return {true, false, common::error::ErrorCode::kOk, "", {}};
        }
        return {true, true, common::error::ErrorCode::kOk, "", it->second};
    }

    bool InvalidatePlayerSnapshot(std::int64_t player_id) override {
        invalidated_players_.push_back(player_id);
        return true;
    }

    game_server::dungeon_runtime::PrepareDungeonEntryPortResponse PrepareDungeonEntry(std::int64_t player_id,
                                                                                std::int64_t session_id,
                                                                                int energy_cost,
                                                                                const std::string& idempotency_key) override {
        ++prepare_calls_;
        prepare_keys_.push_back(idempotency_key);
        if (auto it = prepare_by_key_.find(idempotency_key); it != prepare_by_key_.end()) {
            if (it->second.session_id != session_id) {
                return {false, common::error::ErrorCode::kBattleMismatch, "dungeon entry request mismatch", 0};
            }
            return it->second.response;
        }

        auto it = snapshots_.find(player_id);
        if (it == snapshots_.end()) {
            const game_server::dungeon_runtime::PrepareDungeonEntryPortResponse response{
                false, common::error::ErrorCode::kPlayerNotFound, "player not found", 0};
            prepare_by_key_.emplace(idempotency_key, PreparedEntryRecord{session_id, response});
            return response;
        }

        if (it->second.stamina < energy_cost) {
            const game_server::dungeon_runtime::PrepareDungeonEntryPortResponse response{
                false, common::error::ErrorCode::kStaminaNotEnough, "stamina not enough", 0};
            prepare_by_key_.emplace(idempotency_key, PreparedEntryRecord{session_id, response});
            return response;
        }

        it->second.stamina -= energy_cost;
        const game_server::dungeon_runtime::PrepareDungeonEntryPortResponse response{
            true, common::error::ErrorCode::kOk, "", it->second.stamina};
        prepare_by_key_.emplace(idempotency_key, PreparedEntryRecord{session_id, response});
        return response;
    }

    void SeedPreparedEntry(const std::string& idempotency_key,
                           std::int64_t session_id,
                           int remain_energy) {
        prepare_by_key_[idempotency_key] = PreparedEntryRecord{
            session_id,
            {true, common::error::ErrorCode::kOk, "", remain_energy}};
    }

    void SeedPrepareFailure(const std::string& idempotency_key,
                            std::int64_t session_id,
                            common::error::ErrorCode error_code,
                            std::string error_message) {
        prepare_by_key_[idempotency_key] = PreparedEntryRecord{
            session_id,
            {false, error_code, std::move(error_message), 0}};
    }

    void ClearPreparedEntry(const std::string& idempotency_key) {
        prepare_by_key_.erase(idempotency_key);
    }

    game_server::dungeon_runtime::CancelDungeonEntryPortResponse CancelDungeonEntry(std::int64_t player_id,
                                                                           std::int64_t session_id,
                                                                           int energy_refund,
                                                                           const std::string& idempotency_key) override {
        ++cancel_calls_;
        (void)session_id;
        (void)idempotency_key;
        if (!cancel_should_succeed_) {
            return {false, cancel_error_code_, cancel_error_message_};
        }
        auto it = snapshots_.find(player_id);
        if (it == snapshots_.end()) {
            return {false, common::error::ErrorCode::kPlayerNotFound, "player not found"};
        }
        it->second.stamina += energy_refund;
        return {true, common::error::ErrorCode::kOk, ""};
    }

    game_server::dungeon_runtime::ApplyRewardGrantPortResponse ApplyRewardGrant(
        std::int64_t player_id,
        std::int64_t grant_id,
        std::int64_t session_id,
        const std::vector<Reward>& rewards,
        const std::string& idempotency_key) override {
        ++apply_calls_;
        last_apply_player_id_ = player_id;
        last_apply_grant_id_ = grant_id;
        last_apply_session_id_ = session_id;
        last_apply_rewards_ = rewards;
        last_apply_idempotency_key_ = idempotency_key;
        if (!apply_should_succeed_) {
            return {false, apply_error_code_, apply_error_message_, {}};
        }
        return {true, common::error::ErrorCode::kOk, "", rewards};
    }

    void SetApplyRewardFailure(common::error::ErrorCode code, std::string message) {
        apply_should_succeed_ = false;
        apply_error_code_ = code;
        apply_error_message_ = std::move(message);
    }

    void SetCancelDungeonEntryFailure(common::error::ErrorCode code, std::string message) {
        cancel_should_succeed_ = false;
        cancel_error_code_ = code;
        cancel_error_message_ = std::move(message);
    }

    void SetSnapshotFailure(common::error::ErrorCode code, std::string message) {
        snapshot_should_succeed_ = false;
        snapshot_error_code_ = code;
        snapshot_error_message_ = std::move(message);
    }

    [[nodiscard]] int prepare_calls() const {
        return prepare_calls_;
    }
    [[nodiscard]] int cancel_calls() const {
        return cancel_calls_;
    }
    [[nodiscard]] int apply_calls() const {
        return apply_calls_;
    }
    [[nodiscard]] std::int64_t last_apply_grant_id() const {
        return last_apply_grant_id_;
    }
    [[nodiscard]] const std::vector<Reward>& last_apply_rewards() const {
        return last_apply_rewards_;
    }

private:
    std::unordered_map<std::int64_t, game_server::dungeon_runtime::PlayerSnapshot> snapshots_;
    std::unordered_map<std::string, PreparedEntryRecord> prepare_by_key_;
    std::vector<std::int64_t> invalidated_players_;
    std::vector<std::string> prepare_keys_;
    bool apply_should_succeed_ = true;
    bool cancel_should_succeed_ = true;
    bool snapshot_should_succeed_ = true;
    common::error::ErrorCode apply_error_code_ = common::error::ErrorCode::kOk;
    common::error::ErrorCode cancel_error_code_ = common::error::ErrorCode::kOk;
    common::error::ErrorCode snapshot_error_code_ = common::error::ErrorCode::kOk;
    std::string apply_error_message_;
    std::string cancel_error_message_;
    std::string snapshot_error_message_;
    int prepare_calls_ = 0;
    int cancel_calls_ = 0;
    int apply_calls_ = 0;
    std::int64_t last_apply_player_id_ = 0;
    std::int64_t last_apply_grant_id_ = 0;
    std::int64_t last_apply_session_id_ = 0;
    std::vector<Reward> last_apply_rewards_;
    std::string last_apply_idempotency_key_;
};

class FakePlayerLockRepository final : public game_server::dungeon_runtime::PlayerLockRepository {
public:
    bool Acquire(std::int64_t player_id) override {
        acquired_.push_back(player_id);
        return acquire_ok_;
    }

    void Release(std::int64_t player_id) override {
        released_.push_back(player_id);
    }

    [[nodiscard]] bool WasReleased(std::int64_t player_id) const {
        return std::find(released_.begin(), released_.end(), player_id) != released_.end();
    }

private:
    bool acquire_ok_ = true;
    std::vector<std::int64_t> acquired_;
    std::vector<std::int64_t> released_;
};

class FakeDungeonRepository final : public game_server::dungeon_runtime::DungeonRepository {
public:
    std::optional<DungeonContext> FindDungeonBySessionId(std::int64_t session_id) const override {
        if (const auto it = battles_.find(session_id); it != battles_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::optional<game_server::dungeon_runtime::DungeonEntryOperation> FindDungeonEntryOperationByIdempotencyKey(
        const std::string& idempotency_key) const override {
        if (const auto it = enter_operations_.find(idempotency_key); it != enter_operations_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool CreateDungeonEntryOperation(std::int64_t player_id,
                                    int stage_id,
                                    const std::string& mode,
                                    std::int64_t session_id,
                                    std::int64_t seed,
                                    const std::string& idempotency_key,
                                    std::string* error_message = nullptr) override {
        if (enter_operations_.find(idempotency_key) != enter_operations_.end()) {
            if (error_message != nullptr) {
                *error_message = "dungeon entry operation already exists";
            }
            return false;
        }
        enter_operations_[idempotency_key] = {player_id,
                                              stage_id,
                                              mode,
                                              session_id,
                                              seed,
                                              0,
                                              game_server::dungeon_runtime::DungeonEntryOperationStatus::kPreparing,
                                              common::error::ErrorCode::kOk,
                                              ""};
        return true;
    }

    bool MarkDungeonEntryOperationRolledBack(const std::string& idempotency_key,
                                            std::string* error_message = nullptr) override {
        const auto it = enter_operations_.find(idempotency_key);
        if (it == enter_operations_.end()) {
            if (error_message != nullptr) {
                *error_message = "dungeon entry operation not found";
            }
            return false;
        }
        it->second.status = game_server::dungeon_runtime::DungeonEntryOperationStatus::kRolledBack;
        return true;
    }

    bool MarkDungeonEntryOperationFailed(const std::string& idempotency_key,
                                        common::error::ErrorCode error_code,
                                        const std::string& error_message,
                                        std::string* storage_error_message = nullptr) override {
        (void)storage_error_message;
        const auto it = enter_operations_.find(idempotency_key);
        if (it == enter_operations_.end()) {
            return false;
        }
        it->second.status = game_server::dungeon_runtime::DungeonEntryOperationStatus::kFailed;
        it->second.error_code = error_code;
        it->second.error_message = error_message;
        return true;
    }

    std::optional<DungeonContext> FindActiveDungeonByIdempotencyKey(
        const std::string& idempotency_key) const override {
        if (const auto it = active_battle_by_key_.find(idempotency_key); it != active_battle_by_key_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    std::optional<DungeonContext> FindActiveDungeonByPlayerId(std::int64_t player_id) const override {
        for (const auto& [session_id, battle] : battles_) {
            (void)session_id;
            if (battle.player_id == player_id && !battle.settled) {
                return battle;
            }
        }
        return std::nullopt;
    }

    game_server::dungeon_runtime::EnterDungeonResult CreateDungeonSession(
        std::int64_t session_id,
        std::int64_t player_id,
        int stage_id,
        const std::string& mode,
        int cost_energy,
        int remain_energy_after,
        const std::vector<game_server::dungeon_runtime::DungeonRoleSummary>& role_summaries,
        std::int64_t seed,
        const std::string& idempotency_key,
        const std::string& trace_id) override {
        (void)role_summaries;
        (void)idempotency_key;
        (void)trace_id;
        if (create_battle_session_should_fail_) {
            return {false, create_battle_session_error_, create_battle_session_error_message_, {}};
        }
        DungeonContext context;
        context.session_id = session_id;
        context.player_id = player_id;
        context.stage_id = stage_id;
        context.mode = mode;
        context.cost_energy = cost_energy;
        context.remain_energy_after = remain_energy_after;
        context.seed = seed;
        context.settled = false;
        battles_[session_id] = context;
        active_battle_by_key_[idempotency_key] = context;
        if (auto operation = enter_operations_.find(idempotency_key); operation != enter_operations_.end()) {
            operation->second.remain_energy_after = remain_energy_after;
            operation->second.status = game_server::dungeon_runtime::DungeonEntryOperationStatus::kActive;
        }
        return {true, game_server::dungeon_runtime::DungeonRepositoryError::kNone, "", context};
    }

    bool CancelDungeonSession(std::int64_t session_id, std::string* error_message = nullptr) override {
        ++cancel_session_calls_;
        for (auto key_it = active_battle_by_key_.begin(); key_it != active_battle_by_key_.end();) {
            if (key_it->second.session_id == session_id) {
                key_it = active_battle_by_key_.erase(key_it);
            } else {
                ++key_it;
            }
        }
        if (battles_.erase(session_id) > 0) {
            return true;
        }
        if (error_message != nullptr) {
            *error_message = "dungeon not found";
        }
        return false;
    }

    game_server::dungeon_runtime::SettleDungeonResult RecordDungeonSettlement(std::int64_t session_id,
                                                                       std::int64_t player_id,
                                                                       int stage_id,
                                                                       int result_code,
                                                                       int star,
                                                                       int cost_time_ms,
                                                                       std::int64_t client_score,
                                                                       std::int64_t reward_grant_id,
                                                                       const std::vector<Reward>& rewards,
                                                                       const std::string& idempotency_key) override {
        (void)result_code;
        (void)star;
        (void)idempotency_key;
        ++record_settlement_calls_;
        last_record_cost_time_ms_ = cost_time_ms;
        last_record_client_score_ = client_score;
        if (record_settlement_should_fail_) {
            return {false, game_server::dungeon_runtime::DungeonRepositoryError::kStorageFailure, "record failed"};
        }
        const auto it = battles_.find(session_id);
        if (it == battles_.end()) {
            return {false, game_server::dungeon_runtime::DungeonRepositoryError::kStorageFailure, "dungeon not found"};
        }
        if (it->second.settled) {
            return {false, game_server::dungeon_runtime::DungeonRepositoryError::kDungeonAlreadySettled, "already settled"};
        }

        it->second.player_id = player_id;
        it->second.stage_id = stage_id;
        it->second.settled = true;
        it->second.reward_grant_id = reward_grant_id;
        it->second.grant_status = 0;
        it->second.rewards = rewards;
        for (auto key_it = active_battle_by_key_.begin(); key_it != active_battle_by_key_.end();) {
            if (key_it->second.session_id == session_id) {
                key_it = active_battle_by_key_.erase(key_it);
            } else {
                ++key_it;
            }
        }
        for (auto& [idempotency_key, operation] : enter_operations_) {
            (void)idempotency_key;
            if (operation.session_id == session_id) {
                operation.status = game_server::dungeon_runtime::DungeonEntryOperationStatus::kCompleted;
            }
        }

        reward_grant_owners_[reward_grant_id] = player_id;
        reward_grants_[reward_grant_id] = {true, 0, rewards, game_server::dungeon_runtime::DungeonRepositoryError::kNone, ""};
        return {true, game_server::dungeon_runtime::DungeonRepositoryError::kNone, ""};
    }

    game_server::dungeon_runtime::SettleDungeonResult MarkRewardGrantGranted(std::int64_t reward_grant_id) override {
        ++mark_granted_calls_;
        if (mark_granted_should_fail_) {
            return {false, game_server::dungeon_runtime::DungeonRepositoryError::kStorageFailure, "mark failed"};
        }
        const auto grant_it = reward_grants_.find(reward_grant_id);
        if (grant_it == reward_grants_.end()) {
            return {false, game_server::dungeon_runtime::DungeonRepositoryError::kGrantNotFound, "grant not found"};
        }
        grant_it->second.grant_status = 1;
        for (auto& [session_id, battle] : battles_) {
            (void)session_id;
            if (battle.reward_grant_id == reward_grant_id) {
                battle.grant_status = 1;
            }
        }
        return {true, game_server::dungeon_runtime::DungeonRepositoryError::kNone, ""};
    }

    game_server::dungeon_runtime::RewardGrantStatusResult GetRewardGrantStatus(std::int64_t player_id,
                                                                               std::int64_t reward_grant_id) const override {
        const auto owner_it = reward_grant_owners_.find(reward_grant_id);
        if (owner_it == reward_grant_owners_.end() || owner_it->second != player_id) {
            return {false, 0, {}, game_server::dungeon_runtime::DungeonRepositoryError::kGrantNotFound, "grant not found"};
        }
        if (const auto it = reward_grants_.find(reward_grant_id); it != reward_grants_.end()) {
            return it->second;
        }
        return {false, 0, {}, game_server::dungeon_runtime::DungeonRepositoryError::kGrantNotFound, "grant not found"};
    }

    void SaveDungeon(DungeonContext context) {
        battles_[context.session_id] = std::move(context);
    }

    void SaveRewardGrant(std::int64_t player_id,
                         std::int64_t grant_id,
                         int grant_status,
                         std::vector<Reward> rewards) {
        reward_grant_owners_[grant_id] = player_id;
        reward_grants_[grant_id] =
            {true, grant_status, std::move(rewards), game_server::dungeon_runtime::DungeonRepositoryError::kNone, ""};
    }

    void SetMarkGrantedFailure(bool value) {
        mark_granted_should_fail_ = value;
    }

    void SetCreateDungeonSessionFailure(game_server::dungeon_runtime::DungeonRepositoryError error, std::string message) {
        create_battle_session_should_fail_ = true;
        create_battle_session_error_ = error;
        create_battle_session_error_message_ = std::move(message);
    }

    void SaveEnterOperation(const std::string& idempotency_key,
                            const game_server::dungeon_runtime::DungeonEntryOperation& operation) {
        enter_operations_[idempotency_key] = operation;
    }

    [[nodiscard]] int record_settlement_calls() const {
        return record_settlement_calls_;
    }
    [[nodiscard]] int last_record_cost_time_ms() const {
        return last_record_cost_time_ms_;
    }
    [[nodiscard]] std::int64_t last_record_client_score() const {
        return last_record_client_score_;
    }
    [[nodiscard]] int mark_granted_calls() const {
        return mark_granted_calls_;
    }
    [[nodiscard]] int cancel_session_calls() const {
        return cancel_session_calls_;
    }

private:
    mutable std::unordered_map<std::int64_t, DungeonContext> battles_;
    mutable std::unordered_map<std::string, DungeonContext> active_battle_by_key_;
    mutable std::unordered_map<std::string, game_server::dungeon_runtime::DungeonEntryOperation> enter_operations_;
    mutable std::unordered_map<std::int64_t, game_server::dungeon_runtime::RewardGrantStatusResult> reward_grants_;
    mutable std::unordered_map<std::int64_t, std::int64_t> reward_grant_owners_;
    bool create_battle_session_should_fail_ = false;
    game_server::dungeon_runtime::DungeonRepositoryError create_battle_session_error_ =
        game_server::dungeon_runtime::DungeonRepositoryError::kNone;
    std::string create_battle_session_error_message_;
    bool record_settlement_should_fail_ = false;
    bool mark_granted_should_fail_ = false;
    int record_settlement_calls_ = 0;
    int last_record_cost_time_ms_ = 0;
    std::int64_t last_record_client_score_ = 0;
    int mark_granted_calls_ = 0;
    int cancel_session_calls_ = 0;
};

class FakeDungeonContextRepository final : public game_server::dungeon_runtime::DungeonContextRepository {
public:
    bool Save(const DungeonContext& dungeon_context) override {
        ++save_calls_;
        if (save_should_fail_on_call_ > 0 && save_calls_ == save_should_fail_on_call_) {
            save_should_fail_on_call_ = 0;
            return false;
        }
        contexts_[dungeon_context.session_id] = dungeon_context;
        return true;
    }

    std::optional<DungeonContext> FindBySessionId(std::int64_t session_id) const override {
        if (const auto it = contexts_.find(session_id); it != contexts_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool Delete(std::int64_t session_id) override {
        deleted_session_ids_.push_back(session_id);
        contexts_.erase(session_id);
        return true;
    }

    [[nodiscard]] bool WasDeleted(std::int64_t session_id) const {
        return std::find(deleted_session_ids_.begin(), deleted_session_ids_.end(), session_id) != deleted_session_ids_.end();
    }

    [[nodiscard]] std::optional<DungeonContext> FindSaved(std::int64_t session_id) const {
        if (const auto it = contexts_.find(session_id); it != contexts_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    void SetFailOnSaveCall(int save_call) {
        save_should_fail_on_call_ = save_call;
    }

    [[nodiscard]] int save_calls() const {
        return save_calls_;
    }

private:
    std::unordered_map<std::int64_t, DungeonContext> contexts_;
    std::vector<std::int64_t> deleted_session_ids_;
    int save_should_fail_on_call_ = 0;
    int save_calls_ = 0;
};

bool Expect(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }
    std::cerr << message << '\n';
    return false;
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

std::string BuildSettleTokenForTest(std::int64_t player_id, std::int64_t session_id, int stage_id, std::int64_t seed) {
    std::ostringstream payload;
    payload << player_id << ':' << session_id << ':' << stage_id << ':' << seed;
    const auto raw_payload = payload.str();

    unsigned int digest_length = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];
    if (HMAC(EVP_sha256(),
             "dungeon-settle-token",
             static_cast<int>(std::char_traits<char>::length("dungeon-settle-token")),
             reinterpret_cast<const unsigned char*>(raw_payload.data()),
             raw_payload.size(),
             digest,
             &digest_length) == nullptr) {
        return "";
    }
    return ToHex(digest, digest_length);
}

game_server::dungeon_runtime::EnterDungeonRequest MakeEnterRequest(std::int64_t player_id,
                                                                   std::uint64_t request_id,
                                                                   int stage_id,
                                                                   std::string mode = "pve") {
    game_server::dungeon_runtime::EnterDungeonRequest request;
    request.player_id = player_id;
    request.request_id = request_id;
    request.stage_id = stage_id;
    request.mode = std::move(mode);
    return request;
}

game_server::dungeon_runtime::SettleDungeonRequest MakeSettleRequest(std::int64_t player_id,
                                                                     std::int64_t session_id,
                                                                     int stage_id,
                                                                     int star,
                                                                     int result_code,
                                                                     std::int64_t client_score,
                                                                     std::string settle_token) {
    game_server::dungeon_runtime::SettleDungeonRequest request;
    request.player_id = player_id;
    request.session_id = session_id;
    request.stage_id = stage_id;
    request.star = star;
    request.result_code = result_code;
    request.client_score = client_score;
    request.settle_token = std::move(settle_token);
    return request;
}

game_server::dungeon_runtime::DungeonRuntimeService CreateService(FakePlayerLockRepository& lock_repository,
                                                      FakePlayerSnapshotPort& player_snapshot_port,
                                                      game_server::dungeon_runtime::InMemoryStageConfigRepository& config_repository,
                                                      FakeDungeonRepository& dungeon_repository,
                                                      FakeDungeonContextRepository& dungeon_context_repository) {
    return game_server::dungeon_runtime::DungeonRuntimeService(
        lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);
}

}  // namespace

int main() {
    game_server::dungeon_runtime::PlayerSnapshot player_snapshot;
    player_snapshot.player_id = 20001;
    player_snapshot.level = 10;
    player_snapshot.stamina = 120;
    player_snapshot.role_summaries.push_back({1, 10, 1});

    game_server::dungeon_runtime::StageConfig stage_config;
    stage_config.stage_id = 1001;
    stage_config.required_level = 1;
    stage_config.cost_stamina = 10;
    stage_config.max_star = 3;
    stage_config.normal_gold_reward = 100;
    stage_config.first_clear_diamond_reward = 50;

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        player_snapshot_port.SetSnapshotFailure(common::error::ErrorCode::kStorageError, "snapshot unavailable");
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto enter = battle_service.EnterDungeon(MakeEnterRequest(20001, 90001, 1001), "trace-snapshot-failure");
        if (!Expect(!enter.success && enter.error_code == common::error::ErrorCode::kStorageError,
                    "expected EnterDungeon to surface snapshot port failure as storage error")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto enter = battle_service.EnterDungeon(
            {20001, 90002, 1001, "general", 77, {{3001, 2}, {3002, 1}}},
            "trace-normal");
        if (!Expect(enter.success, "expected EnterDungeon success")) {
            return 1;
        }
        if (!Expect(enter.session_id > 0, "expected generated session_id")) {
            return 1;
        }
        if (!Expect(enter.remain_stamina == 110, "expected stamina to be deducted in EnterDungeon")) {
            return 1;
        }
        const auto active = battle_service.GetActiveDungeon(20001);
        if (!Expect(active.success && active.found, "expected active battle after successful enter")) {
            return 1;
        }
        if (!Expect(active.session_id == enter.session_id, "expected active battle to return same session_id")) {
            return 1;
        }
        if (!Expect(active.stage_id == 1001, "expected active battle to return stage_id")) {
            return 1;
        }
        if (!Expect(active.remain_stamina == 110, "expected active battle to return remain stamina")) {
            return 1;
        }
        if (!Expect(!active.settle_token.empty(), "expected active battle to return settle token")) {
            return 1;
        }
        if (!Expect(active.settle_token == enter.settle_token, "expected settle token to stay stable")) {
            return 1;
        }
        const auto saved_context = dungeon_context_repository.FindSaved(enter.session_id);
        if (!Expect(saved_context.has_value() && saved_context->loadout_id == 77 &&
                        saved_context->use_item_list.size() == 2 &&
                        saved_context->use_item_list[0].item_id == 3001,
                    "expected enter request detail to be saved into dungeon runtime context")) {
            return 1;
        }

        const auto retry_enter = battle_service.EnterDungeon(
            {20001, 90002, 1001, "general", 77, {{3001, 2}, {3002, 1}}},
            "trace-normal-retry");
        if (!Expect(retry_enter.success, "expected EnterDungeon retry success")) {
            return 1;
        }
        if (!Expect(retry_enter.session_id == enter.session_id, "expected EnterDungeon retry to replay same session")) {
            return 1;
        }
        if (!Expect(retry_enter.remain_stamina == enter.remain_stamina,
                    "expected EnterDungeon retry to replay same remain stamina")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.prepare_calls() == 1,
                    "expected EnterDungeon retry to avoid second stamina prepare")) {
            return 1;
        }

        game_server::dungeon_runtime::DungeonBattleStats battle_stats;
        battle_stats.pass_time_seconds = 63;
        battle_stats.dungeon_rank = 2;
        battle_stats.master_combo = 18;
        battle_stats.dungeon_report_json = "{\"combo\":18}";
        const auto settle = battle_service.SettleDungeon(
            {20001, enter.session_id, 1001, 3, 1, 999, enter.settle_token, battle_stats},
            "trace-normal-settle");
        if (!Expect(settle.success, "expected SettleDungeon success")) {
            return 1;
        }
        if (!Expect(settle.grant_status == 1, "expected grant_status=1 for successful settle")) {
            return 1;
        }
        if (!Expect(settle.reward_grant_id == enter.session_id, "expected reward_grant_id=session_id")) {
            return 1;
        }
        if (!Expect(settle.reward_preview.size() == 1, "expected fixed gold reward for MVP settle")) {
            return 1;
        }
        if (!Expect(settle.use_item_list.size() == 2 && settle.use_item_list[1].item_id == 3002,
                    "expected settle response to replay saved use_item_list detail")) {
            return 1;
        }
        if (!Expect(dungeon_repository.record_settlement_calls() == 1, "expected one settlement record call")) {
            return 1;
        }
        if (!Expect(dungeon_repository.last_record_cost_time_ms() == 63000 &&
                        dungeon_repository.last_record_client_score() == 999,
                    "expected settle battle stats to contribute cost_time and score persistence")) {
            return 1;
        }
        if (!Expect(dungeon_repository.mark_granted_calls() == 1, "expected one mark grant call")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.apply_calls() == 1, "expected one apply reward call")) {
            return 1;
        }
        if (!Expect(dungeon_context_repository.WasDeleted(enter.session_id),
                    "expected battle context delete after successful settle")) {
            return 1;
        }
        const auto no_active = battle_service.GetActiveDungeon(20001);
        if (!Expect(no_active.success && !no_active.found, "expected no active battle after settle")) {
            return 1;
        }
        const auto old_request_retry = battle_service.EnterDungeon(
            {20001, 90002, 1001, "general", 77, {{3001, 2}, {3002, 1}}},
            "trace-normal-retry-after-settle");
        if (!Expect(!old_request_retry.success &&
                        old_request_retry.error_code == common::error::ErrorCode::kBattleAlreadySettled,
                    "expected settled enter request_id retry to be rejected")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        player_snapshot_port.SeedPreparedEntry("dungeon-enter:20001:90008", 7008001, 110);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        dungeon_repository.SaveEnterOperation(
            "dungeon-enter:20001:90008",
            {20001,
             1001,
             "pve",
             7008001,
             333001,
             0,
             game_server::dungeon_runtime::DungeonEntryOperationStatus::kPreparing,
             common::error::ErrorCode::kOk,
             ""});
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto enter = battle_service.EnterDungeon(MakeEnterRequest(20001, 90008, 1001), "trace-recover-preparing");
        if (!Expect(enter.success, "expected EnterDungeon to resume preparing operation")) {
            return 1;
        }
        if (!Expect(enter.session_id == 7008001, "expected EnterDungeon to reuse session_id from preparing operation")) {
            return 1;
        }
        if (!Expect(enter.remain_stamina == 110, "expected EnterDungeon to replay prepared remain stamina")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.prepare_calls() == 1,
                    "expected resumed EnterDungeon to reuse the original prepare idempotency")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        player_snapshot_port.SeedPrepareFailure(
            "dungeon-enter:20001:90009", 7009001, common::error::ErrorCode::kStaminaNotEnough, "stamina not enough");
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        dungeon_repository.SaveEnterOperation(
            "dungeon-enter:20001:90009",
            {20001,
             1001,
             "pve",
             7009001,
             333009,
             0,
             game_server::dungeon_runtime::DungeonEntryOperationStatus::kPreparing,
             common::error::ErrorCode::kOk,
             ""});
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto first_enter = battle_service.EnterDungeon(MakeEnterRequest(20001, 90009, 1001), "trace-prepare-failure");
        if (!Expect(!first_enter.success && first_enter.error_code == common::error::ErrorCode::kStaminaNotEnough,
                    "expected prepare failure to preserve original business error")) {
            return 1;
        }

        const auto retry_enter = battle_service.EnterDungeon(
            MakeEnterRequest(20001, 90009, 1001), "trace-prepare-failure-retry");
        if (!Expect(!retry_enter.success && retry_enter.error_code == common::error::ErrorCode::kStaminaNotEnough,
                    "expected same request_id retry to replay original prepare failure")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.prepare_calls() == 1,
                    "expected failed prepare replay to avoid a second prepare call")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        player_snapshot_port.SeedPrepareFailure(
            "dungeon-enter:20001:90010", 7010001, common::error::ErrorCode::kStorageError, "player grpc timeout");
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        dungeon_repository.SaveEnterOperation(
            "dungeon-enter:20001:90010",
            {20001,
             1001,
             "pve",
             7010001,
             333010,
             0,
             game_server::dungeon_runtime::DungeonEntryOperationStatus::kPreparing,
             common::error::ErrorCode::kOk,
             ""});
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto first_enter = battle_service.EnterDungeon(
            MakeEnterRequest(20001, 90010, 1001), "trace-prepare-storage-failure");
        if (!Expect(!first_enter.success && first_enter.error_code == common::error::ErrorCode::kStorageError,
                    "expected temporary prepare storage failure to surface as storage error")) {
            return 1;
        }

        player_snapshot_port.ClearPreparedEntry("dungeon-enter:20001:90010");
        const auto retry_enter = battle_service.EnterDungeon(
            MakeEnterRequest(20001, 90010, 1001), "trace-prepare-storage-retry");
        if (!Expect(retry_enter.success, "expected same request_id retry to recover after temporary prepare failure")) {
            return 1;
        }
        if (!Expect(retry_enter.session_id == 7010001,
                    "expected recovered prepare retry to reuse original session_id")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.prepare_calls() == 2,
                    "expected temporary prepare failure to retry against player service once backend recovers")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        player_snapshot_port.SetCancelDungeonEntryFailure(common::error::ErrorCode::kStorageError, "refund failed");
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        dungeon_repository.SetCreateDungeonSessionFailure(
            game_server::dungeon_runtime::DungeonRepositoryError::kStorageFailure, "insert failed");
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto enter = battle_service.EnterDungeon(
            MakeEnterRequest(20001, 90003, 1001), "trace-enter-compensation-failure");
        if (!Expect(!enter.success && enter.error_code == common::error::ErrorCode::kStorageError,
                    "expected enter battle to surface rollback failure as storage error")) {
            return 1;
        }
        if (!Expect(enter.error_message.find("rollback failed") != std::string::npos,
                    "expected enter battle error message to mention rollback failure")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.cancel_calls() == 3,
                    "expected enter battle failure to retry stamina rollback")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        dungeon_context_repository.SetFailOnSaveCall(1);
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto enter = battle_service.EnterDungeon(
            MakeEnterRequest(20001, 90011, 1001), "trace-enter-runtime-context-save-fail");
        if (!Expect(!enter.success && enter.error_code == common::error::ErrorCode::kStorageError,
                    "expected enter to fail when runtime context save fails")) {
            return 1;
        }
        if (!Expect(dungeon_repository.cancel_session_calls() == 1,
                    "expected runtime context save failure to cancel created dungeon session")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.cancel_calls() == 1,
                    "expected runtime context save failure to refund consumed stamina")) {
            return 1;
        }
        if (!Expect(!dungeon_repository.FindActiveDungeonByPlayerId(20001).has_value(),
                    "expected no active dungeon after runtime context save compensation")) {
            return 1;
        }

        const auto retry_enter = battle_service.EnterDungeon(
            MakeEnterRequest(20001, 90011, 1001), "trace-enter-runtime-context-save-fail-retry");
        if (!Expect(!retry_enter.success && retry_enter.error_message.find("already rolled back") != std::string::npos,
                    "expected same request_id retry to stay blocked after compensated enter failure")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        dungeon_context_repository.SetFailOnSaveCall(2);
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto enter = battle_service.EnterDungeon(
            MakeEnterRequest(20001, 90012, 1001), "trace-settle-runtime-context-pre-save-enter");
        if (!Expect(enter.success, "expected enter success before settle runtime pre-save failure")) {
            return 1;
        }

        const auto settle = battle_service.SettleDungeon(
            MakeSettleRequest(20001, enter.session_id, 1001, 2, 1, 321, enter.settle_token),
            "trace-settle-runtime-context-pre-save");
        if (!Expect(!settle.success && settle.error_code == common::error::ErrorCode::kStorageError,
                    "expected settle to fail when battle stats runtime context save fails")) {
            return 1;
        }
        if (!Expect(dungeon_repository.record_settlement_calls() == 0,
                    "expected settlement write to stop before repository record when runtime pre-save fails")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.apply_calls() == 0,
                    "expected no reward apply when runtime pre-save fails")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        dungeon_context_repository.SetFailOnSaveCall(3);
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto enter = battle_service.EnterDungeon(
            MakeEnterRequest(20001, 90013, 1001), "trace-settle-runtime-context-final-save-enter");
        if (!Expect(enter.success, "expected enter success before settle runtime final-save failure")) {
            return 1;
        }

        const auto first_settle = battle_service.SettleDungeon(
            MakeSettleRequest(20001, enter.session_id, 1001, 3, 1, 654, enter.settle_token),
            "trace-settle-runtime-context-final-save");
        if (!Expect(!first_settle.success && first_settle.error_code == common::error::ErrorCode::kStorageError,
                    "expected settle to fail when settled runtime context save fails")) {
            return 1;
        }
        if (!Expect(dungeon_repository.record_settlement_calls() == 1,
                    "expected settlement to reach repository before settled context save failure")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.apply_calls() == 0,
                    "expected reward apply to wait until settled runtime context is durable")) {
            return 1;
        }
        if (!Expect(dungeon_context_repository.WasDeleted(enter.session_id),
                    "expected stale runtime context to be deleted after settled context save failure")) {
            return 1;
        }

        const auto retry_settle = battle_service.SettleDungeon(
            MakeSettleRequest(20001, enter.session_id, 1001, 3, 1, 654, enter.settle_token),
            "trace-settle-runtime-context-final-save-retry");
        if (!Expect(retry_settle.success && retry_settle.grant_status == 1,
                    "expected settle retry to resume from durable DB state after settled context save failure")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.apply_calls() == 1,
                    "expected retry after settled context save failure to apply reward exactly once")) {
            return 1;
        }
        if (!Expect(dungeon_repository.mark_granted_calls() == 1,
                    "expected retry after settled context save failure to mark grant exactly once")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto enter = battle_service.EnterDungeon(MakeEnterRequest(20001, 90004, 1001), "trace-lose");
        if (!Expect(enter.success, "expected enter success before lose settle")) {
            return 1;
        }

        const auto settle = battle_service.SettleDungeon(
            MakeSettleRequest(20001, enter.session_id, 1001, 0, 0, 0, enter.settle_token), "trace-lose-settle");
        if (!Expect(settle.success, "expected lose settle success")) {
            return 1;
        }
        if (!Expect(settle.reward_preview.empty(), "expected lose settle to grant no rewards")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto enter = battle_service.EnterDungeon(MakeEnterRequest(20001, 90005, 1001), "trace-invalid-lose");
        if (!Expect(enter.success, "expected enter success before invalid lose settle")) {
            return 1;
        }

        const auto settle = battle_service.SettleDungeon(
            MakeSettleRequest(20001, enter.session_id, 1001, 1, 0, 10, enter.settle_token),
            "trace-invalid-lose-settle");
        if (!Expect(!settle.success && settle.error_code == common::error::ErrorCode::kBattleMismatch,
                    "expected invalid lose payload to be rejected")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const std::int64_t session_id = 7001001;
        const std::int64_t reward_grant_id = 88001;
        DungeonContext settled_context;
        settled_context.session_id = session_id;
        settled_context.player_id = 20001;
        settled_context.stage_id = 1001;
        settled_context.mode = "pve";
        settled_context.cost_energy = 10;
        settled_context.remain_energy_after = 110;
        settled_context.seed = 333;
        settled_context.settled = true;
        settled_context.reward_grant_id = reward_grant_id;
        settled_context.grant_status = 0;
        settled_context.rewards = {{"gold", 100}, {"diamond", 50}};
        dungeon_repository.SaveDungeon(settled_context);
        dungeon_repository.SaveRewardGrant(20001, reward_grant_id, 0, {{"gold", 100}, {"diamond", 50}});

        const auto settle = battle_service.SettleDungeon(
            MakeSettleRequest(20001,
                              session_id,
                              1001,
                              3,
                              1,
                              1000,
                              BuildSettleTokenForTest(20001, session_id, 1001, 333)),
            "trace-replay");
        if (!Expect(settle.success, "expected settle replay success when grant_status is pending")) {
            return 1;
        }
        if (!Expect(settle.grant_status == 1, "expected pending grant to be marked granted")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.apply_calls() == 1, "expected replay path to re-apply reward once")) {
            return 1;
        }
        if (!Expect(dungeon_repository.mark_granted_calls() == 1, "expected replay path to mark grant once")) {
            return 1;
        }
        if (!Expect(dungeon_context_repository.WasDeleted(session_id),
                    "expected replay path to clear battle context")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const std::int64_t session_id = 7002001;
        const std::int64_t reward_grant_id = 88002;
        DungeonContext settled_context;
        settled_context.session_id = session_id;
        settled_context.player_id = 20001;
        settled_context.stage_id = 1001;
        settled_context.mode = "pve";
        settled_context.cost_energy = 10;
        settled_context.remain_energy_after = 110;
        settled_context.seed = 444;
        settled_context.settled = true;
        settled_context.reward_grant_id = reward_grant_id;
        settled_context.grant_status = 1;
        settled_context.rewards = {{"gold", 100}, {"diamond", 50}};
        dungeon_repository.SaveDungeon(settled_context);
        dungeon_repository.SaveRewardGrant(20001, reward_grant_id, 1, {{"gold", 100}, {"diamond", 50}});

        const auto settle = battle_service.SettleDungeon(
            MakeSettleRequest(20001,
                              session_id,
                              1001,
                              3,
                              1,
                              1000,
                              BuildSettleTokenForTest(20001, session_id, 1001, 444)),
            "trace-replay-done");
        if (!Expect(settle.success && settle.grant_status == 1,
                    "expected settled replay to return current granted status")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.apply_calls() == 0,
                    "expected settled replay to skip re-applying reward")) {
            return 1;
        }
        if (!Expect(dungeon_repository.mark_granted_calls() == 0,
                    "expected settled replay to skip mark granted")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        player_snapshot_port.SetApplyRewardFailure(common::error::ErrorCode::kStorageError, "apply failed");
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto enter = battle_service.EnterDungeon(MakeEnterRequest(20001, 90006, 1001), "trace-apply-fail-enter");
        if (!Expect(enter.success, "expected enter success before apply failure path")) {
            return 1;
        }

        const auto settle =
            battle_service.SettleDungeon(
                MakeSettleRequest(20001, enter.session_id, 1001, 2, 1, 777, enter.settle_token),
                "trace-apply-fail-settle");
        if (!Expect(!settle.success, "expected settle failure when ApplyRewardGrant fails")) {
            return 1;
        }
        if (!Expect(settle.error_code == common::error::ErrorCode::kStorageError,
                    "expected apply failure to propagate storage error")) {
            return 1;
        }
        if (!Expect(dungeon_repository.record_settlement_calls() == 1,
                    "expected settlement to be recorded before apply failure")) {
            return 1;
        }
        if (!Expect(dungeon_repository.mark_granted_calls() == 0,
                    "expected mark granted not called when apply fails")) {
            return 1;
        }

        const auto grant_status = battle_service.GetSettlementGrantStatus(20001, enter.session_id);
        if (!Expect(grant_status.success && grant_status.grant_status == 0,
                    "expected grant_status to remain pending after apply failure")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        dungeon_repository.SetMarkGrantedFailure(true);
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto enter = battle_service.EnterDungeon(MakeEnterRequest(20001, 90007, 1001), "trace-mark-fail-enter");
        if (!Expect(enter.success, "expected enter success before mark failure path")) {
            return 1;
        }

        const auto settle =
            battle_service.SettleDungeon(
                MakeSettleRequest(20001, enter.session_id, 1001, 1, 1, 123, enter.settle_token),
                "trace-mark-fail-settle");
        if (!Expect(!settle.success, "expected settle failure when MarkRewardGrantGranted fails")) {
            return 1;
        }
        if (!Expect(settle.error_code == common::error::ErrorCode::kStorageError,
                    "expected mark failure to surface storage error")) {
            return 1;
        }
        if (!Expect(player_snapshot_port.apply_calls() == 1, "expected reward apply attempted before mark failure")) {
            return 1;
        }
        if (!Expect(dungeon_repository.mark_granted_calls() == 1, "expected one mark attempt")) {
            return 1;
        }

        const auto grant_status = battle_service.GetSettlementGrantStatus(20001, enter.session_id);
        if (!Expect(grant_status.success && grant_status.grant_status == 0,
                    "expected grant_status to remain pending when mark fails")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        const auto status = battle_service.GetSettlementGrantStatus(20001, 123456789);
        if (!Expect(!status.success && status.error_code == common::error::ErrorCode::kBattleNotFound,
                    "expected missing reward grant status to map to battle not found")) {
            return 1;
        }
    }

    {
        FakePlayerSnapshotPort player_snapshot_port(player_snapshot);
        FakePlayerLockRepository lock_repository;
        game_server::dungeon_runtime::InMemoryStageConfigRepository config_repository(stage_config);
        FakeDungeonRepository dungeon_repository;
        FakeDungeonContextRepository dungeon_context_repository;
        auto battle_service = CreateService(
            lock_repository, player_snapshot_port, config_repository, dungeon_repository, dungeon_context_repository);

        dungeon_repository.SaveRewardGrant(20002, 99123, 1, {{"gold", 100}});
        const auto status = battle_service.GetSettlementGrantStatus(20001, 99123);
        if (!Expect(!status.success && status.error_code == common::error::ErrorCode::kBattleNotFound,
                    "expected reward grant status lookup to enforce player ownership")) {
            return 1;
        }
    }

    return 0;
}

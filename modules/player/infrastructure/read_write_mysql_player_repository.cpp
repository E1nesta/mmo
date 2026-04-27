// 代码规范落地：基础设施层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/player/infrastructure/read_write_mysql_player_repository.h"

#include "runtime/foundation/log/logger.h"

#include <algorithm>
#include <sstream>

namespace game_server::player {

namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

int ChapterIdFromStageId(int stage_id) {
    return stage_id > 0 ? stage_id / 1000 : 0;
}

std::string ShardSuffix(std::int64_t player_id) {
    const auto shard = static_cast<unsigned int>(player_id & 0x0F);
    std::string suffix = "00";
    suffix[0] = kHexDigits[(shard >> 4) & 0x0F];
    suffix[1] = kHexDigits[shard & 0x0F];
    return suffix;
}

std::string ProfileTable(std::int64_t player_id) {
    return "player_profile_" + ShardSuffix(player_id);
}

std::string CurrencyTable(std::int64_t player_id) {
    return "player_currency_" + ShardSuffix(player_id);
}

std::string RoleTable(std::int64_t player_id) {
    return "player_role_" + ShardSuffix(player_id);
}

std::string StageProgressTable() {
    return "player_stage_progress";
}

void SortCurrencies(std::vector<common::model::CurrencyBalance>* currencies) {
    if (currencies == nullptr) {
        return;
    }
    std::sort(currencies->begin(), currencies->end(), [](const auto& left, const auto& right) {
        return left.currency_type < right.currency_type;
    });
}

}  // namespace

ReadWriteMySqlPlayerRepository::ReadWriteMySqlPlayerRepository(common::mysql::MySqlReadWriteClientPool& mysql_pool)
    : mysql_pool_(&mysql_pool),
      writer_repository_(mysql_pool.Writer()) {}

// 状态读取：`LoadPlayerState` 负责加载上下文并返回稳定结果。
std::optional<common::model::PlayerState> ReadWriteMySqlPlayerRepository::LoadPlayerState(std::int64_t player_id) const {
    std::string reader_error;
    const auto reader_result = LoadPlayerStateFromPool(mysql_pool_->ReaderOrWriter(), player_id, &reader_error);
    if (!reader_error.empty()) {
        writer_fallback_count_.fetch_add(1, std::memory_order_relaxed);
        common::log::Logger::Instance().Log(
            common::log::LogLevel::kWarn,
            "player repository reader unavailable, fallback to writer: player_id=" + std::to_string(player_id) +
                " error=" + reader_error);
        std::string writer_error;
        const auto writer_result = LoadPlayerStateFromPool(mysql_pool_->Writer(), player_id, &writer_error);
        if (!writer_error.empty()) {
            common::log::Logger::Instance().Log(
                common::log::LogLevel::kWarn,
                "player repository writer load failed: player_id=" + std::to_string(player_id) + " error=" +
                    writer_error);
        }
        return writer_result;
    }

    if (mysql_pool_->HasReader()) {
        reader_hit_count_.fetch_add(1, std::memory_order_relaxed);
    }
    return reader_result;
}

BattleEntrySnapshotResult ReadWriteMySqlPlayerRepository::GetBattleEntrySnapshot(std::int64_t player_id) const {
    return writer_repository_.GetBattleEntrySnapshot(player_id);
}

PrepareBattleEntryResult ReadWriteMySqlPlayerRepository::PrepareBattleEntry(std::int64_t player_id,
                                                                            std::int64_t session_id,
                                                                            int energy_cost,
                                                                            const std::string& idempotency_key) {
    return writer_repository_.PrepareBattleEntry(player_id, session_id, energy_cost, idempotency_key);
}

CancelBattleEntryResult ReadWriteMySqlPlayerRepository::CancelBattleEntry(std::int64_t player_id,
                                                                          std::int64_t session_id,
                                                                          int energy_refund,
                                                                          const std::string& idempotency_key) {
    return writer_repository_.CancelBattleEntry(player_id, session_id, energy_refund, idempotency_key);
}

ApplyRewardGrantResult ReadWriteMySqlPlayerRepository::ApplyRewardGrant(
    std::int64_t player_id,
    std::int64_t grant_id,
    std::int64_t session_id,
    const std::vector<common::model::Reward>& rewards,
    const std::string& idempotency_key) {
    return writer_repository_.ApplyRewardGrant(player_id, grant_id, session_id, rewards, idempotency_key);
}

std::uint64_t ReadWriteMySqlPlayerRepository::ReaderHitCount() const {
    return reader_hit_count_.load(std::memory_order_relaxed);
}

std::uint64_t ReadWriteMySqlPlayerRepository::WriterFallbackCount() const {
    return writer_fallback_count_.load(std::memory_order_relaxed);
}

// 状态读取：`LoadPlayerStateFromPool` 负责加载上下文并返回稳定结果。
std::optional<common::model::PlayerState> ReadWriteMySqlPlayerRepository::LoadPlayerStateFromPool(
    common::mysql::MySqlClientPool& mysql_pool,
    std::int64_t player_id,
    std::string* error_message) const {
    if (error_message != nullptr) {
        error_message->clear();
    }

    auto mysql = mysql_pool.Acquire();

    std::ostringstream profile_sql;
    profile_sql << "SELECT player_id, account_id, server_id, nickname, level, energy, main_stage_id, fight_power "
                   "FROM "
                << ProfileTable(player_id) << " WHERE player_id = " << player_id << " LIMIT 1";
    std::string query_error;
    const auto profile_row = mysql->QueryOne(profile_sql.str(), &query_error);
    if (!query_error.empty()) {
        if (error_message != nullptr) {
            *error_message = query_error;
        }
        return std::nullopt;
    }
    if (!profile_row.has_value()) {
        return std::nullopt;
    }

    common::model::PlayerState state;
    state.profile.player_id = std::stoll(profile_row->at("player_id"));
    state.profile.account_id = std::stoll(profile_row->at("account_id"));
    state.profile.server_id = std::stoi(profile_row->at("server_id"));
    state.profile.nickname = profile_row->at("nickname");
    state.profile.player_name = state.profile.nickname;
    state.profile.level = std::stoi(profile_row->at("level"));
    state.profile.stamina = std::stoi(profile_row->at("energy"));
    state.profile.main_stage_id = std::stoi(profile_row->at("main_stage_id"));
    state.profile.main_chapter_id = ChapterIdFromStageId(state.profile.main_stage_id);
    state.profile.fight_power = std::stoll(profile_row->at("fight_power"));

    std::ostringstream currency_sql;
    currency_sql << "SELECT currency_type, amount FROM " << CurrencyTable(player_id) << " WHERE player_id = "
                 << player_id;
    const auto currency_rows = mysql->Query(currency_sql.str(), &query_error);
    if (!query_error.empty()) {
        if (error_message != nullptr) {
            *error_message = query_error;
        }
        return std::nullopt;
    }
    for (const auto& row : currency_rows) {
        common::model::CurrencyBalance currency;
        currency.currency_type = row.at("currency_type");
        currency.amount = std::stoll(row.at("amount"));
        state.currencies.push_back(currency);
        if (currency.currency_type == "gold") {
            state.profile.gold = currency.amount;
        } else if (currency.currency_type == "diamond") {
            state.profile.diamond = currency.amount;
        }
    }
    SortCurrencies(&state.currencies);

    std::ostringstream role_sql;
    role_sql << "SELECT role_id, level, star FROM " << RoleTable(player_id) << " WHERE player_id = " << player_id
             << " ORDER BY role_id ASC LIMIT 8";
    const auto role_rows = mysql->Query(role_sql.str(), &query_error);
    if (!query_error.empty()) {
        if (error_message != nullptr) {
            *error_message = query_error;
        }
        return std::nullopt;
    }
    for (const auto& row : role_rows) {
        state.role_summaries.push_back({std::stoi(row.at("role_id")), std::stoi(row.at("level")), std::stoi(row.at("star"))});
    }

    std::ostringstream progress_sql;
    progress_sql << "SELECT stage_id, best_star, is_first_clear FROM " << StageProgressTable()
                 << " WHERE player_id = " << player_id << " ORDER BY stage_id ASC";
    const auto progress_rows = mysql->Query(progress_sql.str(), &query_error);
    if (!query_error.empty()) {
        if (error_message != nullptr) {
            *error_message = query_error;
        }
        return std::nullopt;
    }
    for (const auto& row : progress_rows) {
        state.stage_progress.push_back(
            {std::stoi(row.at("stage_id")), std::stoi(row.at("best_star")), row.at("is_first_clear") != "0"});
    }

    return state;
}

}  // namespace game_server::player

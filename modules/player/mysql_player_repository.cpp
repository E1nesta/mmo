#include "modules/player/mysql_player_repository.h"

#include <cstdlib>
#include <sstream>
#include <utility>

namespace mmo::modules::player {
namespace {

std::string quote(const std::string& value) {
    std::string output;
    output.reserve(value.size() + 2);
    output.push_back('\'');
    for (const char ch : value) {
        if (ch == '\'' || ch == '\\') {
            output.push_back('\\');
        }
        output.push_back(ch);
    }
    output.push_back('\'');
    return output;
}

}  // namespace

MysqlPlayerRepository::MysqlPlayerRepository(
    std::shared_ptr<mmo::runtime::storage::MysqlClient> client)
    : client_(std::move(client)) {}

std::optional<PlayerProfile> MysqlPlayerRepository::load_profile(std::int64_t player_id) {
    std::ostringstream sql;
    sql << "SELECT player_id, gold, exp FROM player_profile WHERE player_id="
        << player_id << " LIMIT 1";

    std::vector<std::vector<std::string>> rows;
    std::string error_message;
    if (!client_->query(sql.str(), &rows, &error_message) || rows.empty()) {
        return std::nullopt;
    }

    const auto& row = rows.front();
    if (row.size() < 3) {
        return std::nullopt;
    }

    PlayerProfile profile;
    profile.player_id = std::strtoll(row[0].c_str(), nullptr, 10);
    profile.gold = std::strtoll(row[1].c_str(), nullptr, 10);
    profile.exp = std::strtoll(row[2].c_str(), nullptr, 10);
    return profile;
}

bool MysqlPlayerRepository::save_profile(
    const PlayerProfile& profile,
    std::string* error_message) {
    std::ostringstream sql;
    sql << "INSERT INTO player_profile(player_id, gold, exp) VALUES("
        << profile.player_id << ", " << profile.gold << ", " << profile.exp
        << ") ON DUPLICATE KEY UPDATE gold=VALUES(gold), exp=VALUES(exp)";
    return client_->execute(sql.str(), error_message);
}

bool MysqlPlayerRepository::record_reward_ledger(
    const RewardLedgerRecord& record,
    std::string* error_message) {
    std::ostringstream sql;
    sql << "INSERT INTO reward_ledger(player_id, idempotency_key, request_id, gold, exp)"
        << " VALUES("
        << record.player_id << ", "
        << quote(record.idempotency_key) << ", "
        << quote(record.request_id) << ", "
        << record.gold << ", "
        << record.exp << ")";
    return client_->execute(sql.str(), error_message);
}

}  // namespace mmo::modules::player

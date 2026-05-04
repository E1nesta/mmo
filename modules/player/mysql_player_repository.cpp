#include "modules/player/mysql_player_repository.h"

#include <cstdlib>
#include <sstream>
#include <utility>

namespace modules::player {
namespace {

constexpr unsigned int kMysqlDuplicateKey = 1062;

std::string quote(
    const runtime::storage::MysqlClient& client,
    const std::string& value) {
    return "'" + client.escape_string(value) + "'";
}

struct RewardDelta {
    std::int64_t gold{};
    std::int64_t exp{};
};

RewardDelta sum_rewards(const std::vector<Reward>& rewards) {
    RewardDelta delta;
    for (const auto& reward : rewards) {
        if (reward.type == "gold") {
            delta.gold += reward.amount;
        } else if (reward.type == "exp") {
            delta.exp += reward.amount;
        }
    }
    return delta;
}

std::optional<PlayerProfile> load_profile_with_client(
    runtime::storage::MysqlClient& client,
    std::int64_t player_id,
    std::string* error_message) {
    std::ostringstream sql;
    sql << "SELECT player_id, gold, exp FROM player_profiles WHERE player_id="
        << player_id << " LIMIT 1";

    std::vector<std::vector<std::string>> rows;
    if (!client.query(sql.str(), &rows, error_message) || rows.empty()) {
        return std::nullopt;
    }

    const auto& row = rows.front();
    if (row.size() < 3) {
        if (error_message != nullptr) {
            *error_message = "player profile row is malformed";
        }
        return std::nullopt;
    }

    PlayerProfile profile;
    profile.player_id = std::strtoll(row[0].c_str(), nullptr, 10);
    profile.gold = std::strtoll(row[1].c_str(), nullptr, 10);
    profile.exp = std::strtoll(row[2].c_str(), nullptr, 10);
    return profile;
}

}  // namespace

MysqlPlayerRepository::MysqlPlayerRepository(
    std::shared_ptr<runtime::storage::MysqlConnectionPool> pool)
    : pool_(std::move(pool)) {}

MysqlPlayerRepository::MysqlPlayerRepository(
    std::shared_ptr<runtime::storage::MysqlClient> client)
    : client_(std::move(client)) {}

std::optional<PlayerProfile> MysqlPlayerRepository::load_profile(std::int64_t player_id) {
    auto client = acquire_client();
    if (client == nullptr) {
        return std::nullopt;
    }
    std::string error_message;
    return load_profile_with_client(*client, player_id, &error_message);
}

bool MysqlPlayerRepository::save_profile(
    const PlayerProfile& profile,
    std::string* error_message) {
    auto client = acquire_client();
    if (client == nullptr) {
        if (error_message != nullptr) {
            *error_message = "mysql player repository is not initialized";
        }
        return false;
    }
    std::ostringstream sql;
    sql << "INSERT INTO player_profiles(player_id, gold, exp) VALUES("
        << profile.player_id << ", " << profile.gold << ", " << profile.exp
        << ") ON DUPLICATE KEY UPDATE gold=VALUES(gold), exp=VALUES(exp)";
    return client->execute(sql.str(), error_message);
}

bool MysqlPlayerRepository::record_reward_ledger(
    const RewardLedgerRecord& record,
    std::string* error_message) {
    auto client = acquire_client();
    if (client == nullptr) {
        if (error_message != nullptr) {
            *error_message = "mysql player repository is not initialized";
        }
        return false;
    }
    std::ostringstream sql;
    sql << "INSERT INTO reward_ledger(player_id, idempotency_key, request_id, gold, exp)"
        << " VALUES("
        << record.player_id << ", "
        << quote(*client, record.idempotency_key) << ", "
        << quote(*client, record.request_id) << ", "
        << record.gold << ", "
        << record.exp << ")";
    return client->execute(sql.str(), error_message);
}

bool MysqlPlayerRepository::apply_reward_once(
    std::int64_t player_id,
    const std::string& idempotency_key,
    const std::vector<Reward>& rewards,
    ApplyRewardResult* result,
    std::string* error_message) {
    if (result == nullptr) {
        if (error_message != nullptr) {
            *error_message = "apply reward result output is null";
        }
        return false;
    }
    auto client = acquire_client();
    if (client == nullptr) {
        if (error_message != nullptr) {
            *error_message = "mysql player repository is not initialized";
        }
        return false;
    }
    if (player_id <= 0 || idempotency_key.empty()) {
        if (error_message != nullptr) {
            *error_message = "reward request is invalid";
        }
        return false;
    }

    const auto delta = sum_rewards(rewards);
    if (!client->begin_transaction(error_message)) {
        return false;
    }

    std::ostringstream ensure_profile;
    ensure_profile << "INSERT INTO player_profiles(player_id, gold, exp) VALUES("
                   << player_id
                   << ", 0, 0) ON DUPLICATE KEY UPDATE player_id=VALUES(player_id)";
    if (!client->execute(ensure_profile.str(), error_message)) {
        client->rollback(nullptr);
        return false;
    }

    std::ostringstream insert_ledger;
    insert_ledger
        << "INSERT INTO reward_ledger(player_id, idempotency_key, request_id, gold, exp)"
        << " VALUES("
        << player_id << ", "
        << quote(*client, idempotency_key) << ", "
        << quote(*client, idempotency_key) << ", "
        << delta.gold << ", "
        << delta.exp << ")";
    if (!client->execute(insert_ledger.str(), error_message)) {
        if (client->last_error_code() == kMysqlDuplicateKey) {
            client->rollback(nullptr);
            const auto profile =
                load_profile_with_client(*client, player_id, error_message);
            if (!profile.has_value()) {
                return false;
            }
            result->success = true;
            result->applied = false;
            result->gold = profile->gold;
            result->exp = profile->exp;
            return true;
        }
        client->rollback(nullptr);
        return false;
    }

    std::ostringstream update_profile;
    update_profile << "UPDATE player_profiles SET gold=gold+(" << delta.gold
                   << "), exp=exp+(" << delta.exp
                   << ") WHERE player_id=" << player_id;
    if (!client->execute(update_profile.str(), error_message)) {
        client->rollback(nullptr);
        return false;
    }

    const auto profile = load_profile_with_client(*client, player_id, error_message);
    if (!profile.has_value()) {
        client->rollback(nullptr);
        return false;
    }

    if (!client->commit(error_message)) {
        client->rollback(nullptr);
        return false;
    }

    result->success = true;
    result->applied = true;
    result->gold = profile->gold;
    result->exp = profile->exp;
    return true;
}

std::shared_ptr<runtime::storage::MysqlClient>
MysqlPlayerRepository::acquire_client() {
    if (pool_ != nullptr) {
        return pool_->acquire();
    }
    return client_;
}

}  // namespace modules::player

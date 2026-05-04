#pragma once

#include <memory>
#include <optional>
#include <string>

#include "modules/player/player_repository.h"
#include "runtime/storage/mysql_client.h"
#include "runtime/storage/mysql_connection_pool.h"

namespace modules::player {

class MysqlPlayerRepository : public PlayerRepository {
public:
    explicit MysqlPlayerRepository(
        std::shared_ptr<runtime::storage::MysqlConnectionPool> pool);
    explicit MysqlPlayerRepository(
        std::shared_ptr<runtime::storage::MysqlClient> client);

    std::optional<PlayerProfile> load_profile(std::int64_t player_id) override;
    bool save_profile(const PlayerProfile& profile, std::string* error_message) override;
    bool record_reward_ledger(
        const RewardLedgerRecord& record,
        std::string* error_message) override;
    bool apply_reward_once(
        std::int64_t player_id,
        const std::string& idempotency_key,
        const std::vector<Reward>& rewards,
        ApplyRewardResult* result,
        std::string* error_message) override;

private:
    std::shared_ptr<runtime::storage::MysqlClient> acquire_client();

    std::shared_ptr<runtime::storage::MysqlConnectionPool> pool_;
    std::shared_ptr<runtime::storage::MysqlClient> client_;
};

}  // namespace modules::player

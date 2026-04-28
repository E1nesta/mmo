#pragma once

#include <memory>
#include <optional>
#include <string>

#include "modules/player/player_repository.h"
#include "runtime/storage/mysql_client.h"

namespace mmo::modules::player {

class MysqlPlayerRepository : public PlayerRepository {
public:
    explicit MysqlPlayerRepository(
        std::shared_ptr<mmo::runtime::storage::MysqlClient> client);

    std::optional<PlayerProfile> load_profile(std::int64_t player_id) override;
    bool save_profile(const PlayerProfile& profile, std::string* error_message) override;
    bool record_reward_ledger(
        const RewardLedgerRecord& record,
        std::string* error_message) override;

private:
    std::shared_ptr<mmo::runtime::storage::MysqlClient> client_;
};

}  // namespace mmo::modules::player

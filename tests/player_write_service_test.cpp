#include "modules/player/application/player_write_service.h"
#include "modules/player/domain/reward.h"
#include "modules/player/infrastructure/in_memory_player_repository.h"

#include <fstream>
#include <iostream>
#include <unordered_map>

namespace {

class InMemoryPlayerCacheRepository final : public game_server::player::PlayerCacheRepository {
public:
    bool Save(const common::model::PlayerState& player_state) override {
        cache_[player_state.profile.player_id] = player_state;
        ++save_count_;
        return true;
    }

    std::optional<common::model::PlayerState> FindByPlayerId(std::int64_t player_id) const override {
        const auto iter = cache_.find(player_id);
        if (iter == cache_.end()) {
            return std::nullopt;
        }
        return iter->second;
    }

    bool Invalidate(std::int64_t player_id) override {
        cache_.erase(player_id);
        ++invalidate_count_;
        return true;
    }

    [[nodiscard]] int SaveCount() const { return save_count_; }
    [[nodiscard]] int InvalidateCount() const { return invalidate_count_; }

private:
    std::unordered_map<std::int64_t, common::model::PlayerState> cache_;
    int save_count_ = 0;
    int invalidate_count_ = 0;
};

bool Expect(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

}  // namespace

int main() {
    common::config::SimpleConfig config;
    const std::string config_path = "player_write_service_test.conf";
    {
        std::ofstream output(config_path);
        output << "demo.account_id=10001\n";
        output << "demo.player_id=20001\n";
        output << "demo.player_name=hero_demo\n";
        output << "demo.level=10\n";
        output << "demo.stamina=120\n";
        output << "demo.gold=1000\n";
        output << "demo.diamond=100\n";
    }

    if (!config.LoadFromFile(config_path)) {
        std::cerr << "failed to load player write test config\n";
        return 1;
    }

    auto player_repository = game_server::player::InMemoryPlayerRepository::FromConfig(config);
    InMemoryPlayerCacheRepository cache_repository;
    game_server::player::PlayerWriteService player_write_service(player_repository, cache_repository);

    constexpr std::int64_t session_id = 2000110011;
    const auto entry = player_write_service.PrepareBattleEntry(20001, session_id, 10, "battle-enter:test");
    if (!Expect(entry.success && entry.remain_energy == 110, "expected prepare battle entry to spend stamina")) {
        return 1;
    }
    if (!Expect(cache_repository.InvalidateCount() == 1,
                "expected prepare battle entry to invalidate player cache")) {
        return 1;
    }

    const std::vector<common::model::Reward> rewards = {{"gold", 100}, {"diamond", 50}};
    const auto applied =
        player_write_service.ApplyRewardGrant(20001, session_id, session_id, rewards, "battle-settle:test");
    if (!Expect(applied.success && applied.applied_currencies.size() == 2, "expected reward grant to succeed")) {
        return 1;
    }
    if (!Expect(cache_repository.InvalidateCount() == 2 && cache_repository.SaveCount() == 0,
                "expected write service to invalidate cache instead of eagerly refreshing it")) {
        return 1;
    }

    const auto snapshot = player_write_service.GetBattleEntrySnapshot(20001);
    if (!Expect(snapshot.success && snapshot.found && snapshot.energy == 110,
                "expected battle entry snapshot to reflect remaining energy")) {
        return 1;
    }

    return 0;
}

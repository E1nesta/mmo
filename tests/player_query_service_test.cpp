#include "modules/player/application/player_query_service.h"
#include "modules/player/infrastructure/in_memory_player_repository.h"

#include <fstream>
#include <iostream>
#include <unordered_map>

namespace {

class InMemoryPlayerCacheRepository final : public game_server::player::PlayerCacheRepository {
public:
    bool Save(const common::model::PlayerState& player_state) override {
        cache_[player_state.profile.player_id] = player_state;
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
        return true;
    }

private:
    std::unordered_map<std::int64_t, common::model::PlayerState> cache_;
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
    const std::string config_path = "player_query_service_test.conf";
    {
        std::ofstream output(config_path);
        output << "demo.account_id=10001\n";
        output << "demo.player_id=20001\n";
        output << "demo.player_name=hero_demo\n";
        output << "demo.nickname=hero_demo\n";
        output << "demo.server_id=1\n";
        output << "demo.level=10\n";
        output << "demo.stamina=120\n";
        output << "demo.gold=1000\n";
        output << "demo.diamond=100\n";
        output << "demo.main_stage_id=1001\n";
        output << "demo.fight_power=1200\n";
    }

    if (!config.LoadFromFile(config_path)) {
        std::cerr << "failed to load player query test config\n";
        return 1;
    }

    auto player_repository = game_server::player::InMemoryPlayerRepository::FromConfig(config);
    InMemoryPlayerCacheRepository cache_repository;
    game_server::player::PlayerQueryService player_query_service(player_repository, cache_repository);

    const auto first_load = player_query_service.LoadPlayer(20001);
    if (!Expect(first_load.success && !first_load.loaded_from_cache, "expected first query load from storage")) {
        return 1;
    }
    if (!Expect(first_load.home_init.player_display.player_id == 20001 &&
                    first_load.home_init.player_display.nickname == "hero_demo" &&
                    first_load.home_init.resource_summary.stamina == 120 &&
                    first_load.home_init.character_summary.role_summaries.size() == 3 &&
                    first_load.home_init.dungeon_summary.main_stage_id == 1001 &&
                    first_load.home_init.dungeon_summary.stage_progress_count == 1,
                "expected first query load to expose home init blocks")) {
        return 1;
    }

    const auto second_load = player_query_service.LoadPlayer(20001);
    if (!Expect(second_load.success && second_load.loaded_from_cache, "expected second query load from cache")) {
        return 1;
    }

    const auto invalidate = player_query_service.InvalidatePlayerCache(20001);
    if (!Expect(invalidate.success, "expected player cache invalidation to succeed")) {
        return 1;
    }

    const auto third_load = player_query_service.LoadPlayer(20001);
    if (!Expect(third_load.success && !third_load.loaded_from_cache,
                "expected query load after invalidate to fall back to storage")) {
        return 1;
    }

    const auto snapshot = player_query_service.GetPlayerSnapshot(20001);
    if (!Expect(snapshot.success && snapshot.found && snapshot.player_id == 20001,
                "expected player snapshot lookup to succeed")) {
        return 1;
    }
    if (!Expect(snapshot.nickname == "hero_demo" && snapshot.gold == 1000 && snapshot.diamond == 100 &&
                    snapshot.main_stage_id == 1001 && snapshot.main_chapter_id == 1 &&
                    snapshot.fight_power == 1200 && snapshot.stage_progress_count == 1 &&
                    snapshot.role_summaries.size() == 3 && snapshot.currencies.size() == 2,
                "expected player snapshot to expose home summary fields")) {
        return 1;
    }

    const auto missing = player_query_service.LoadPlayer(99999);
    if (!Expect(!missing.success && missing.error_code == common::error::ErrorCode::kPlayerNotFound,
                "expected missing player query load to fail")) {
        return 1;
    }

    return 0;
}

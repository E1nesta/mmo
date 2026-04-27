// 代码规范落地：基础设施层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/dungeon_runtime/infrastructure/redis_dungeon_context_repository.h"

namespace game_server::dungeon_runtime {

namespace {

std::string SerializeUseItems(const std::vector<DungeonUseItem>& use_item_list) {
    std::string output;
    bool first = true;
    for (const auto& item : use_item_list) {
        if (!first) {
            output.push_back(';');
        }
        output.append(std::to_string(item.item_id));
        output.push_back(':');
        output.append(std::to_string(item.amount));
        first = false;
    }
    return output;
}

std::vector<DungeonUseItem> ParseUseItems(const std::string& raw) {
    std::vector<DungeonUseItem> output;
    std::size_t cursor = 0;
    while (cursor < raw.size()) {
        const auto next = raw.find(';', cursor);
        const auto part = raw.substr(cursor, next == std::string::npos ? std::string::npos : next - cursor);
        if (!part.empty()) {
            const auto colon = part.find(':');
            if (colon != std::string::npos) {
                output.push_back({std::stoi(part.substr(0, colon)), std::stoi(part.substr(colon + 1))});
            }
        }
        if (next == std::string::npos) {
            break;
        }
        cursor = next + 1;
    }
    return output;
}

}  // namespace

RedisDungeonContextRepository RedisDungeonContextRepository::FromConfig(common::redis::RedisClientPool& redis_pool,
                                                                      const common::config::SimpleConfig& config) {
    return RedisDungeonContextRepository(redis_pool, config.GetInt("storage.battle.context_ttl_seconds", 3600));
}

RedisDungeonContextRepository::RedisDungeonContextRepository(common::redis::RedisClientPool& redis_pool, int ttl_seconds)
    : redis_pool_(redis_pool), ttl_seconds_(ttl_seconds) {}

// 状态推进：`Save` 执行写链或补偿并收敛状态变化。
bool RedisDungeonContextRepository::Save(const DungeonContext& dungeon_context) {
    auto redis = redis_pool_.Acquire();
    return redis->HSet(CacheKey(dungeon_context.session_id),
                       {{"player_id", std::to_string(dungeon_context.player_id)},
                        {"stage_id", std::to_string(dungeon_context.stage_id)},
                        {"mode", dungeon_context.mode},
                        {"loadout_id", std::to_string(dungeon_context.loadout_id)},
                        {"use_item_list", SerializeUseItems(dungeon_context.use_item_list)},
                        {"cost_energy", std::to_string(dungeon_context.cost_energy)},
                        {"remain_energy_after", std::to_string(dungeon_context.remain_energy_after)},
                        {"seed", std::to_string(dungeon_context.seed)},
                        {"pass_time_seconds", std::to_string(dungeon_context.battle_stats.pass_time_seconds)},
                        {"dungeon_rank", std::to_string(dungeon_context.battle_stats.dungeon_rank)},
                        {"master_be_hit", std::to_string(dungeon_context.battle_stats.master_be_hit)},
                        {"use_ougi", std::to_string(dungeon_context.battle_stats.use_ougi)},
                        {"dodge_seconds", std::to_string(dungeon_context.battle_stats.dodge_seconds)},
                        {"heal_sum", std::to_string(dungeon_context.battle_stats.heal_sum)},
                        {"be_harm", std::to_string(dungeon_context.battle_stats.be_harm)},
                        {"kill_enemy", std::to_string(dungeon_context.battle_stats.kill_enemy)},
                        {"master_combo", std::to_string(dungeon_context.battle_stats.master_combo)},
                        {"dungeon_report_json", dungeon_context.battle_stats.dungeon_report_json},
                        {"settled", dungeon_context.settled ? "1" : "0"},
                        {"reward_grant_id", std::to_string(dungeon_context.reward_grant_id)},
                        {"grant_status", std::to_string(dungeon_context.grant_status)}},
                       ttl_seconds_);
}

// 状态读取：`FindBySessionId` 负责加载上下文并返回稳定结果。
std::optional<DungeonContext> RedisDungeonContextRepository::FindBySessionId(std::int64_t session_id) const {
    auto redis = redis_pool_.Acquire();
    const auto values = redis->HGetAll(CacheKey(session_id));
    if (!values.has_value() || values->empty()) {
        return std::nullopt;
    }

    DungeonContext dungeon_context;
    dungeon_context.session_id = session_id;
    dungeon_context.player_id = std::stoll(values->at("player_id"));
    dungeon_context.stage_id = std::stoi(values->at("stage_id"));
    dungeon_context.mode = values->at("mode");
    if (const auto iter = values->find("loadout_id"); iter != values->end()) {
        dungeon_context.loadout_id = std::stoi(iter->second);
    }
    if (const auto iter = values->find("use_item_list"); iter != values->end()) {
        dungeon_context.use_item_list = ParseUseItems(iter->second);
    }
    dungeon_context.cost_energy = std::stoi(values->at("cost_energy"));
    if (const auto iter = values->find("remain_energy_after"); iter != values->end()) {
        dungeon_context.remain_energy_after = std::stoi(iter->second);
    }
    dungeon_context.seed = std::stoll(values->at("seed"));
    if (const auto iter = values->find("pass_time_seconds"); iter != values->end()) {
        dungeon_context.battle_stats.pass_time_seconds = std::stoi(iter->second);
    }
    if (const auto iter = values->find("dungeon_rank"); iter != values->end()) {
        dungeon_context.battle_stats.dungeon_rank = std::stoi(iter->second);
    }
    if (const auto iter = values->find("master_be_hit"); iter != values->end()) {
        dungeon_context.battle_stats.master_be_hit = std::stoi(iter->second);
    }
    if (const auto iter = values->find("use_ougi"); iter != values->end()) {
        dungeon_context.battle_stats.use_ougi = std::stoi(iter->second);
    }
    if (const auto iter = values->find("dodge_seconds"); iter != values->end()) {
        dungeon_context.battle_stats.dodge_seconds = std::stoi(iter->second);
    }
    if (const auto iter = values->find("heal_sum"); iter != values->end()) {
        dungeon_context.battle_stats.heal_sum = std::stoi(iter->second);
    }
    if (const auto iter = values->find("be_harm"); iter != values->end()) {
        dungeon_context.battle_stats.be_harm = std::stoi(iter->second);
    }
    if (const auto iter = values->find("kill_enemy"); iter != values->end()) {
        dungeon_context.battle_stats.kill_enemy = std::stoi(iter->second);
    }
    if (const auto iter = values->find("master_combo"); iter != values->end()) {
        dungeon_context.battle_stats.master_combo = std::stoi(iter->second);
    }
    if (const auto iter = values->find("dungeon_report_json"); iter != values->end()) {
        dungeon_context.battle_stats.dungeon_report_json = iter->second;
    }
    dungeon_context.settled = values->at("settled") == "1";
    dungeon_context.reward_grant_id = std::stoll(values->at("reward_grant_id"));
    dungeon_context.grant_status = std::stoi(values->at("grant_status"));
    return dungeon_context;
}

bool RedisDungeonContextRepository::Delete(std::int64_t session_id) {
    auto redis = redis_pool_.Acquire();
    return redis->Del(CacheKey(session_id));
}

std::string RedisDungeonContextRepository::CacheKey(std::int64_t session_id) {
    return "dungeon:ctx:" + std::to_string(session_id);
}

}  // namespace game_server::dungeon_runtime

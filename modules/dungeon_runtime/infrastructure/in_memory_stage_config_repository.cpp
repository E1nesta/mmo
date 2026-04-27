// 代码规范落地：基础设施层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "modules/dungeon_runtime/infrastructure/in_memory_stage_config_repository.h"

namespace game_server::dungeon_runtime {

InMemoryStageConfigRepository InMemoryStageConfigRepository::FromConfig(const common::config::SimpleConfig& config) {
    StageConfig stage_config;
    stage_config.chapter_id = config.GetInt("demo.chapter_id", config.GetInt("demo.stage_id", 1001) / 1000);
    stage_config.stage_id = config.GetInt("demo.stage_id", 1001);
    stage_config.required_level = config.GetInt("demo.stage_required_level", 1);
    stage_config.cost_stamina = config.GetInt("demo.stage_cost_stamina", 10);
    stage_config.max_star = config.GetInt("demo.stage_max_star", 3);
    stage_config.normal_gold_reward = config.GetInt("demo.stage_normal_gold_reward", 100);
    stage_config.first_clear_diamond_reward = config.GetInt("demo.stage_first_clear_diamond_reward", 50);
    return InMemoryStageConfigRepository(stage_config);
}

InMemoryStageConfigRepository::InMemoryStageConfigRepository(StageConfig stage_config)
    : stage_config_(stage_config) {}

// 状态读取：`FindByStageId` 负责加载上下文并返回稳定结果。
std::optional<StageConfig> InMemoryStageConfigRepository::FindByStageId(int stage_id) const {
    if (stage_id != stage_config_.stage_id) {
        return std::nullopt;
    }
    return stage_config_;
}

}  // namespace game_server::dungeon_runtime

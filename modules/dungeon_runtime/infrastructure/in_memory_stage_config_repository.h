// 代码规范落地：基础设施层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/config/simple_config.h"
#include "modules/dungeon_runtime/ports/stage_config_repository.h"

namespace game_server::dungeon_runtime {

class InMemoryStageConfigRepository final : public StageConfigRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    static InMemoryStageConfigRepository FromConfig(const common::config::SimpleConfig& config);

    explicit InMemoryStageConfigRepository(StageConfig stage_config);

    [[nodiscard]] std::optional<StageConfig> FindByStageId(int stage_id) const override;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    StageConfig stage_config_;
};

}  // namespace game_server::dungeon_runtime

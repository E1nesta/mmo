// 代码规范落地：端口契约层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/dungeon_runtime/domain/stage_config.h"

#include <optional>

namespace game_server::dungeon_runtime {

class StageConfigRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    virtual ~StageConfigRepository() = default;

    [[nodiscard]] virtual std::optional<StageConfig> FindByStageId(int stage_id) const = 0;
};

}  // namespace game_server::dungeon_runtime

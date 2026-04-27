// 代码规范落地：端口契约层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/dungeon_runtime/domain/dungeon_context.h"

#include <cstdint>
#include <optional>
#include <string>

namespace game_server::dungeon_runtime {

// 运行态副本上下文持久化边界。
class DungeonContextRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    virtual ~DungeonContextRepository() = default;

    virtual bool Save(const DungeonContext& dungeon_context) = 0;
    [[nodiscard]] virtual std::optional<DungeonContext> FindBySessionId(std::int64_t session_id) const = 0;
    virtual bool Delete(std::int64_t session_id) = 0;
};

}  // namespace game_server::dungeon_runtime

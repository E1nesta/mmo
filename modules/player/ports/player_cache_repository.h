// 代码规范落地：端口契约层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "modules/player/domain/player_state.h"

#include <cstdint>
#include <optional>

namespace game_server::player {

// 存储边界：隔离应用编排与底层读写实现。
class PlayerCacheRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    virtual ~PlayerCacheRepository() = default;

    virtual bool Save(const common::model::PlayerState& player_state) = 0;
    [[nodiscard]] virtual std::optional<common::model::PlayerState> FindByPlayerId(std::int64_t player_id) const = 0;
    virtual bool Invalidate(std::int64_t player_id) = 0;
};

}  // namespace game_server::player

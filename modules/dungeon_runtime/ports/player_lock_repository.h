// 代码规范落地：端口契约层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <cstdint>

namespace game_server::dungeon_runtime {

// 玩家级并发控制的存储边界。
class PlayerLockRepository {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    virtual ~PlayerLockRepository() = default;

    virtual bool Acquire(std::int64_t player_id) = 0;
    virtual void Release(std::int64_t player_id) = 0;
};

}  // namespace game_server::dungeon_runtime

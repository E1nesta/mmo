// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace common::id {

class IdGenerator {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    explicit IdGenerator(std::uint16_t node_id = 1);

    [[nodiscard]] std::int64_t Next();
    [[nodiscard]] std::string NextString();

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    std::uint16_t node_id_ = 1;
    std::atomic<std::uint64_t> sequence_{0};
};

}  // namespace common::id

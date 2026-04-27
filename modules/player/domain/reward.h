// 代码规范落地：领域模型层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <cstdint>
#include <string>

namespace common::model {

struct Reward {
    std::string reward_type;
    std::int64_t amount = 0;
};

}  // namespace common::model

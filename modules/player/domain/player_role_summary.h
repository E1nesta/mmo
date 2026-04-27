// 代码规范落地：领域模型层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

namespace common::model {

struct PlayerRoleSummary {
    int role_id = 0;
    int level = 1;
    int star = 1;
};

}  // namespace common::model

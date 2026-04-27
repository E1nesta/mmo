// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <optional>
#include <string>

namespace common::security {

// 生成不透明会话标识，用于对外可见的会话令牌。
class SessionToken {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    [[nodiscard]] static std::optional<std::string> GenerateHex(std::size_t num_bytes = 32);
};

}  // namespace common::security

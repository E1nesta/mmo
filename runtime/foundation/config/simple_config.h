// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace common::config {

class SimpleConfig {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    bool LoadFromFile(const std::string& path);

    [[nodiscard]] bool Contains(const std::string& key) const;
    [[nodiscard]] std::string GetString(const std::string& key, const std::string& default_value = "") const;
    [[nodiscard]] int GetInt(const std::string& key, int default_value = 0) const;
    [[nodiscard]] bool GetBool(const std::string& key, bool default_value = false) const;

    [[nodiscard]] const std::unordered_map<std::string, std::string>& Values() const;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] static std::string ExpandEnvironmentVariables(const std::string& value);

    std::unordered_map<std::string, std::string> values_;
};

}  // namespace common::config

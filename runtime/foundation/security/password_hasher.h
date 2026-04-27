// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <optional>
#include <string>

namespace common::security {

class PasswordHasher {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    [[nodiscard]] static std::string HashPassword(const std::string& password,
                                                  const std::string& salt,
                                                  int iterations = 100000);
    [[nodiscard]] static bool VerifyPassword(const std::string& password, const std::string& encoded_hash);
    [[nodiscard]] static std::optional<std::string> BuildEncodedHash(const std::string& password,
                                                                     const std::string& salt,
                                                                     int iterations = 100000);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] static std::string ToHex(const unsigned char* data, std::size_t size);
    [[nodiscard]] static std::optional<std::string> FromHex(const std::string& hex);
};

}  // namespace common::security

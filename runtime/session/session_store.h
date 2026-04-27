// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/session/session.h"

#include <cstdint>
#include <optional>
#include <string>

namespace common::session {

// 会话读边界：供各服务读取并恢复会话上下文。
class SessionReader {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    virtual ~SessionReader() = default;

    [[nodiscard]] virtual std::optional<common::model::Session> FindById(const std::string& session_id) const = 0;
};

// 会话写边界：负责创建、撤销与设备绑定等会话状态变更。
class SessionStore : public SessionReader {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    ~SessionStore() override = default;

    virtual common::model::Session Create(std::int64_t account_id, std::int64_t player_id) = 0;
    virtual bool RevokeById(const std::string& session_id) = 0;
    virtual bool BindDeviceId(const std::string& session_id, const std::string& device_id) = 0;
};

}  // namespace common::session

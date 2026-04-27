// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/session/session_store.h"

#include <mutex>
#include <unordered_map>

namespace common::session {

class InMemorySessionStore final : public SessionStore {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    InMemorySessionStore() = default;

    common::model::Session Create(std::int64_t account_id, std::int64_t player_id) override;
    [[nodiscard]] std::optional<common::model::Session> FindById(const std::string& session_id) const override;
    bool RevokeById(const std::string& session_id) override;
    bool BindDeviceId(const std::string& session_id, const std::string& device_id) override;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, common::model::Session> sessions_;
    std::unordered_map<std::int64_t, std::string> account_sessions_;
};

}  // namespace common::session

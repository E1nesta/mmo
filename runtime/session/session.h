// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <cstdint>
#include <string>

namespace common::model {

enum class SessionStatus {
    kActive = 1,
    kRevoked = 2,
};

struct Session {
    std::string session_id;
    std::int64_t account_id = 0;
    std::int64_t player_id = 0;
    std::int64_t created_at_epoch_seconds = 0;
    std::int64_t expires_at_epoch_seconds = 0;
    SessionStatus status = SessionStatus::kActive;
    std::string device_id;
    std::int64_t revoked_at_epoch_seconds = 0;
};

}  // namespace common::model

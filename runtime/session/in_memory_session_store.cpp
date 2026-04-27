// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/session/in_memory_session_store.h"

#include "runtime/foundation/security/session_token.h"

#include <chrono>
#include <stdexcept>

namespace common::session {

// 状态推进：`Create` 执行写链或补偿并收敛状态变化。
common::model::Session InMemorySessionStore::Create(std::int64_t account_id, std::int64_t player_id) {
    const auto now = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    common::model::Session session;
    session.account_id = account_id;
    session.player_id = player_id;
    session.created_at_epoch_seconds = now;
    session.expires_at_epoch_seconds = session.created_at_epoch_seconds + 3600;
    session.status = common::model::SessionStatus::kActive;

    const auto session_token = common::security::SessionToken::GenerateHex();
    if (!session_token.has_value()) {
        throw std::runtime_error("failed to generate secure session token");
    }
    session.session_id = *session_token;

    std::lock_guard<std::mutex> lock(mutex_);
    const auto active_iter = account_sessions_.find(account_id);
    if (active_iter != account_sessions_.end()) {
        if (auto existing = sessions_.find(active_iter->second); existing != sessions_.end()) {
            existing->second.status = common::model::SessionStatus::kRevoked;
            existing->second.revoked_at_epoch_seconds = now;
        }
    }
    sessions_[session.session_id] = session;
    account_sessions_[account_id] = session.session_id;
    return session;
}

// 状态读取：`FindById` 负责加载上下文并返回稳定结果。
std::optional<common::model::Session> InMemorySessionStore::FindById(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto iter = sessions_.find(session_id);
    if (iter == sessions_.end()) {
        return std::nullopt;
    }
    return iter->second;
}

bool InMemorySessionStore::RevokeById(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto iter = sessions_.find(session_id);
    if (iter == sessions_.end()) {
        return false;
    }
    iter->second.status = common::model::SessionStatus::kRevoked;
    iter->second.revoked_at_epoch_seconds = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    const auto account_iter = account_sessions_.find(iter->second.account_id);
    if (account_iter != account_sessions_.end() && account_iter->second == session_id) {
        account_sessions_.erase(account_iter);
    }
    return true;
}

bool InMemorySessionStore::BindDeviceId(const std::string& session_id, const std::string& device_id) {
    if (device_id.empty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    const auto iter = sessions_.find(session_id);
    if (iter == sessions_.end()) {
        return false;
    }
    iter->second.device_id = device_id;
    return true;
}

}  // namespace common::session

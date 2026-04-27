// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/session/redis_session_store.h"

#include "runtime/foundation/log/logger.h"
#include "runtime/foundation/security/session_token.h"

#include <chrono>
#include <stdexcept>

namespace common::session {

RedisSessionStore RedisSessionStore::FromConfig(common::redis::RedisClientPool& redis_pool,
                                                const common::config::SimpleConfig& config) {
    return RedisSessionStore(redis_pool, config.GetInt("storage.session.ttl_seconds", 3600));
}

RedisSessionStore::RedisSessionStore(common::redis::RedisClientPool& redis_pool, int session_ttl_seconds)
    : redis_pool_(redis_pool), session_ttl_seconds_(session_ttl_seconds) {}

// 状态推进：`Create` 执行写链或补偿并收敛状态变化。
common::model::Session RedisSessionStore::Create(std::int64_t account_id, std::int64_t player_id) {
    auto redis = redis_pool_.Acquire();
    common::model::Session session;
    session.account_id = account_id;
    session.player_id = player_id;
    session.created_at_epoch_seconds = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    session.expires_at_epoch_seconds = session.created_at_epoch_seconds + session_ttl_seconds_;

    const auto session_token = common::security::SessionToken::GenerateHex();
    if (!session_token.has_value()) {
        throw std::runtime_error("failed to generate secure session token");
    }
    session.session_id = *session_token;
    session.status = common::model::SessionStatus::kActive;

    const auto account_key = AccountSessionKey(account_id);
    if (const auto old_session = redis->Get(account_key); old_session.has_value()) {
        (void)RevokeById(*old_session);
    }

    redis->HSet(SessionKey(session.session_id),
                {{"account_id", std::to_string(session.account_id)},
                 {"player_id", std::to_string(session.player_id)},
                 {"created_at_epoch_seconds", std::to_string(session.created_at_epoch_seconds)},
                 {"expires_at_epoch_seconds", std::to_string(session.expires_at_epoch_seconds)},
                 {"status", std::to_string(static_cast<int>(session.status))},
                 {"device_id", session.device_id},
                 {"revoked_at_epoch_seconds", "0"}},
                session_ttl_seconds_);
    redis->Set(account_key, session.session_id, session_ttl_seconds_);
    common::log::Logger::Instance().Log(
        common::log::LogLevel::kInfo,
        "session created: account_id=" + std::to_string(account_id) + " player_id=" + std::to_string(player_id) +
            " session_id=" + session.session_id);
    return session;
}

// 状态读取：`FindById` 负责加载上下文并返回稳定结果。
std::optional<common::model::Session> RedisSessionStore::FindById(const std::string& session_id) const {
    auto redis = redis_pool_.Acquire();
    const auto values = redis->HGetAll(SessionKey(session_id));
    if (!values.has_value() || values->empty()) {
        return std::nullopt;
    }

    common::model::Session session;
    session.session_id = session_id;
    session.account_id = std::stoll(values->at("account_id"));
    session.player_id = std::stoll(values->at("player_id"));
    session.created_at_epoch_seconds = std::stoll(values->at("created_at_epoch_seconds"));
    const auto expires_iter = values->find("expires_at_epoch_seconds");
    if (expires_iter != values->end()) {
        session.expires_at_epoch_seconds = std::stoll(expires_iter->second);
    } else {
        session.expires_at_epoch_seconds = session.created_at_epoch_seconds + session_ttl_seconds_;
    }
    if (const auto status_iter = values->find("status"); status_iter != values->end()) {
        session.status = std::stoi(status_iter->second) == 2 ? common::model::SessionStatus::kRevoked
                                                             : common::model::SessionStatus::kActive;
    }
    if (const auto device_iter = values->find("device_id"); device_iter != values->end()) {
        session.device_id = device_iter->second;
    }
    if (const auto revoked_iter = values->find("revoked_at_epoch_seconds"); revoked_iter != values->end() &&
        !revoked_iter->second.empty()) {
        session.revoked_at_epoch_seconds = std::stoll(revoked_iter->second);
    }
    return session;
}

bool RedisSessionStore::RevokeById(const std::string& session_id) {
    auto redis = redis_pool_.Acquire();
    const auto session = FindById(session_id);
    if (!session.has_value()) {
        return false;
    }
    const auto now = static_cast<std::int64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    std::string error_message;
    if (!redis->HSet(SessionKey(session_id),
                     {{"status", std::to_string(static_cast<int>(common::model::SessionStatus::kRevoked))},
                      {"revoked_at_epoch_seconds", std::to_string(now)}},
                     session_ttl_seconds_,
                     &error_message)) {
        return false;
    }
    const auto account_key = AccountSessionKey(session->account_id);
    if (const auto active_session = redis->Get(account_key); active_session.has_value() && *active_session == session_id) {
        (void)redis->Del(account_key, &error_message);
    }
    common::log::Logger::Instance().Log(
        common::log::LogLevel::kInfo,
        "session revoked: account_id=" + std::to_string(session->account_id) + " player_id=" +
            std::to_string(session->player_id) + " session_id=" + session_id);
    return true;
}

bool RedisSessionStore::BindDeviceId(const std::string& session_id, const std::string& device_id) {
    if (device_id.empty()) {
        return false;
    }
    auto redis = redis_pool_.Acquire();
    std::string error_message;
    return redis->HSet(SessionKey(session_id), {{"device_id", device_id}}, session_ttl_seconds_, &error_message);
}

std::string RedisSessionStore::SessionKey(const std::string& session_id) {
    return "session:" + session_id;
}

std::string RedisSessionStore::AccountSessionKey(std::int64_t account_id) {
    return "account:session:" + std::to_string(account_id);
}

}  // namespace common::session

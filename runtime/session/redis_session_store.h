// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/config/simple_config.h"
#include "runtime/storage/redis/redis_client_pool.h"
#include "runtime/session/session_store.h"

namespace common::session {

// 基于 Redis 的实现：承接对应边界的数据读写与状态维护。
class RedisSessionStore final : public SessionStore {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    static RedisSessionStore FromConfig(common::redis::RedisClientPool& redis_pool,
                                        const common::config::SimpleConfig& config);

    RedisSessionStore(common::redis::RedisClientPool& redis_pool, int session_ttl_seconds);

    common::model::Session Create(std::int64_t account_id, std::int64_t player_id) override;
    [[nodiscard]] std::optional<common::model::Session> FindById(const std::string& session_id) const override;
    bool RevokeById(const std::string& session_id) override;
    bool BindDeviceId(const std::string& session_id, const std::string& device_id) override;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] static std::string SessionKey(const std::string& session_id);
    [[nodiscard]] static std::string AccountSessionKey(std::int64_t account_id);

    common::redis::RedisClientPool& redis_pool_;
    int session_ttl_seconds_ = 3600;
};

}  // namespace common::session

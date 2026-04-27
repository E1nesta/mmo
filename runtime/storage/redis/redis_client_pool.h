// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/storage/redis/redis_client.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace common::redis {

class RedisClientPool {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    class Lease {
    // 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
    public:
        Lease() = default;
        Lease(RedisClientPool* pool, std::unique_ptr<RedisClient> client);
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        Lease(Lease&& other) noexcept;
        Lease& operator=(Lease&& other) noexcept;
        ~Lease();

        RedisClient& operator*() const;
        RedisClient* operator->() const;
        [[nodiscard]] bool HasValue() const;

    // 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
    private:
        void Reset();

        RedisClientPool* pool_ = nullptr;
        std::unique_ptr<RedisClient> client_;
    };

    RedisClientPool(ConnectionOptions options, std::size_t pool_size);

    bool Initialize(std::string* error_message = nullptr);
    Lease Acquire();
    std::optional<Lease> TryAcquireFor(std::chrono::milliseconds timeout, std::string* error_message = nullptr);
    void Return(std::unique_ptr<RedisClient> client);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    ConnectionOptions options_;
    std::size_t pool_size_ = 1;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::unique_ptr<RedisClient>> available_;
};

}  // namespace common::redis

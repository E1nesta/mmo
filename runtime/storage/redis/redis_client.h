// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/config/simple_config.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

struct redisContext;

namespace common::redis {

struct ConnectionOptions {
    std::string host = "127.0.0.1";
    int port = 6379;
    std::string password;
    int database = 0;
    int timeout_ms = 2000;
};

struct PoolOptions {
    ConnectionOptions connection;
    std::size_t pool_size = 4;
};

ConnectionOptions ReadConnectionOptions(const config::SimpleConfig& config);
ConnectionOptions ReadConnectionOptions(const config::SimpleConfig& config, const std::string& prefix);
ConnectionOptions ReadConnectionOptionsWithFallback(const config::SimpleConfig& config,
                                                    const std::string& preferred_prefix,
                                                    const std::string& fallback_prefix);
std::size_t ReadPoolSize(const config::SimpleConfig& config, const std::string& prefix, std::size_t default_value = 4);
std::size_t ReadPoolSizeWithFallback(const config::SimpleConfig& config,
                                     const std::string& preferred_prefix,
                                     const std::string& fallback_prefix,
                                     std::size_t default_value = 4);
PoolOptions ReadPoolOptionsWithFallback(const config::SimpleConfig& config,
                                        const std::string& preferred_prefix,
                                        const std::string& fallback_prefix,
                                        std::size_t default_pool_size = 4);

class RedisClient {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    explicit RedisClient(ConnectionOptions options);
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    bool Connect(std::string* error_message = nullptr);
    [[nodiscard]] bool IsConnected() const;
    bool Ping(std::string* error_message = nullptr);

    bool Set(const std::string& key, const std::string& value, int ttl_seconds = 0, std::string* error_message = nullptr);
    [[nodiscard]] std::optional<std::string> Get(const std::string& key, std::string* error_message = nullptr);
    bool Del(const std::string& key, std::string* error_message = nullptr);
    bool HSet(const std::string& key,
              const std::unordered_map<std::string, std::string>& values,
              int ttl_seconds = 0,
              std::string* error_message = nullptr);
    [[nodiscard]] std::optional<std::unordered_map<std::string, std::string>> HGetAll(const std::string& key,
                                                                                       std::string* error_message = nullptr);
    bool SetNxWithExpire(const std::string& key,
                         const std::string& value,
                         int ttl_seconds,
                         bool* inserted,
                         std::string* error_message = nullptr);
    bool IncrementWithExpire(const std::string& key,
                             int ttl_seconds,
                             std::int64_t* value,
                             std::string* error_message = nullptr);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] std::string LastError() const;

    ConnectionOptions options_;
    redisContext* context_ = nullptr;
};

}  // namespace common::redis

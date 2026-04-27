// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/config/simple_config.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct MYSQL;

namespace common::mysql {

struct ConnectionOptions {
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string user = "game";
    std::string password = "gamepass";
    std::string database = "game_backend";
    std::string charset = "utf8mb4";
};

struct PoolOptions {
    ConnectionOptions connection;
    std::size_t pool_size = 4;
};

struct ReadWritePoolOptions {
    PoolOptions writer;
    std::optional<PoolOptions> reader;
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
ReadWritePoolOptions ReadReadWritePoolOptions(const config::SimpleConfig& config,
                                              const std::string& base_prefix,
                                              std::size_t default_pool_size = 4);

using Row = std::unordered_map<std::string, std::string>;

class MySqlClient {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    explicit MySqlClient(ConnectionOptions options);
    ~MySqlClient();

    MySqlClient(const MySqlClient&) = delete;
    MySqlClient& operator=(const MySqlClient&) = delete;

    bool Connect(std::string* error_message = nullptr);
    [[nodiscard]] bool IsConnected() const;
    bool Ping(std::string* error_message = nullptr);

    [[nodiscard]] std::string Escape(const std::string& value);

    bool Execute(const std::string& sql, std::string* error_message = nullptr, std::uint64_t* affected_rows = nullptr);
    [[nodiscard]] std::vector<Row> Query(const std::string& sql, std::string* error_message = nullptr);
    [[nodiscard]] std::optional<Row> QueryOne(const std::string& sql, std::string* error_message = nullptr);

    bool BeginTransaction(std::string* error_message = nullptr);
    bool Commit(std::string* error_message = nullptr);
    bool Rollback(std::string* error_message = nullptr);

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    [[nodiscard]] std::string LastError() const;

    ConnectionOptions options_;
    MYSQL* handle_ = nullptr;
};

}  // namespace common::mysql

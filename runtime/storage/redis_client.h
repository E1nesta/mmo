#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <hiredis/hiredis.h>

namespace mmo::runtime::storage {

struct RedisConfig {
    std::string host{"127.0.0.1"};
    std::uint16_t port{6379};
    int database{};
    std::string password;
    int timeout_millis{1000};
};

class RedisClient {
public:
    RedisClient() = default;
    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;
    ~RedisClient();

    bool connect(const RedisConfig& config, std::string* error_message);
    bool set(const std::string& key, const std::string& value, std::string* error_message);
    bool set_with_ttl_millis(
        const std::string& key,
        const std::string& value,
        std::uint64_t ttl_millis,
        std::string* error_message);
    bool set_if_absent_with_ttl_millis(
        const std::string& key,
        const std::string& value,
        std::uint64_t ttl_millis,
        bool* stored,
        std::string* error_message);
    std::optional<std::string> get(const std::string& key, std::string* error_message);
    bool remove(const std::string& key, std::string* error_message);
    bool expire(const std::string& key, int seconds, std::string* error_message);
    bool is_connected() const;
    void close();

private:
    redisContext* context_{};
};

}  // namespace mmo::runtime::storage

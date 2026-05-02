#include "runtime/storage/redis_client.h"

#include <chrono>
#include <cstdlib>

namespace mmo::runtime::storage {
namespace {

struct ReplyGuard {
    explicit ReplyGuard(redisReply* reply_value) : reply(reply_value) {}
    ReplyGuard(const ReplyGuard&) = delete;
    ReplyGuard& operator=(const ReplyGuard&) = delete;
    ~ReplyGuard() {
        if (reply != nullptr) {
            freeReplyObject(reply);
        }
    }

    redisReply* reply{};
};

bool reply_ok(redisReply* reply, std::string* error_message) {
    if (reply == nullptr) {
        if (error_message != nullptr) {
            *error_message = "redis command returned null";
        }
        return false;
    }
    if (reply->type == REDIS_REPLY_ERROR) {
        if (error_message != nullptr) {
            *error_message = reply->str != nullptr ? reply->str : "redis error";
        }
        return false;
    }
    return true;
}

}  // namespace

RedisClient::~RedisClient() {
    close();
}

bool RedisClient::connect(const RedisConfig& config, std::string* error_message) {
    timeval timeout{};
    timeout.tv_sec = config.timeout_millis / 1000;
    timeout.tv_usec = (config.timeout_millis % 1000) * 1000;

    context_ = redisConnectWithTimeout(config.host.c_str(), config.port, timeout);
    if (context_ == nullptr) {
        if (error_message != nullptr) {
            *error_message = "redisConnectWithTimeout returned null";
        }
        return false;
    }
    if (context_->err != 0) {
        if (error_message != nullptr) {
            *error_message = context_->errstr;
        }
        close();
        return false;
    }
    if (!config.password.empty()) {
        ReplyGuard auth_reply(static_cast<redisReply*>(
            redisCommand(context_, "AUTH %b", config.password.data(), config.password.size())));
        if (!reply_ok(auth_reply.reply, error_message)) {
            close();
            return false;
        }
    }
    if (config.database > 0) {
        ReplyGuard select_reply(static_cast<redisReply*>(
            redisCommand(context_, "SELECT %d", config.database)));
        if (!reply_ok(select_reply.reply, error_message)) {
            close();
            return false;
        }
    }
    return true;
}

bool RedisClient::set(
    const std::string& key,
    const std::string& value,
    std::string* error_message) {
    if (!is_connected()) {
        if (error_message != nullptr) {
            *error_message = "redis client is not connected";
        }
        return false;
    }
    auto* raw_reply = static_cast<redisReply*>(
        redisCommand(context_, "SET %b %b", key.data(), key.size(), value.data(), value.size()));
    ReplyGuard reply(raw_reply);
    return reply_ok(reply.reply, error_message);
}

bool RedisClient::set_with_ttl_millis(
    const std::string& key,
    const std::string& value,
    std::uint64_t ttl_millis,
    std::string* error_message) {
    if (!is_connected()) {
        if (error_message != nullptr) {
            *error_message = "redis client is not connected";
        }
        return false;
    }
    if (ttl_millis == 0) {
        if (error_message != nullptr) {
            *error_message = "redis ttl must be greater than zero";
        }
        return false;
    }
    auto* raw_reply = static_cast<redisReply*>(redisCommand(
        context_,
        "SET %b %b PX %llu",
        key.data(),
        key.size(),
        value.data(),
        value.size(),
        static_cast<unsigned long long>(ttl_millis)));
    ReplyGuard reply(raw_reply);
    return reply_ok(reply.reply, error_message);
}

bool RedisClient::set_if_absent_with_ttl_millis(
    const std::string& key,
    const std::string& value,
    std::uint64_t ttl_millis,
    bool* stored,
    std::string* error_message) {
    if (stored == nullptr) {
        if (error_message != nullptr) {
            *error_message = "redis stored output is null";
        }
        return false;
    }
    *stored = false;
    if (!is_connected()) {
        if (error_message != nullptr) {
            *error_message = "redis client is not connected";
        }
        return false;
    }
    if (ttl_millis == 0) {
        if (error_message != nullptr) {
            *error_message = "redis ttl must be greater than zero";
        }
        return false;
    }
    auto* raw_reply = static_cast<redisReply*>(redisCommand(
        context_,
        "SET %b %b NX PX %llu",
        key.data(),
        key.size(),
        value.data(),
        value.size(),
        static_cast<unsigned long long>(ttl_millis)));
    ReplyGuard reply(raw_reply);
    if (!reply_ok(reply.reply, error_message)) {
        return false;
    }
    if (reply.reply->type == REDIS_REPLY_NIL) {
        *stored = false;
        return true;
    }
    *stored = true;
    return true;
}

std::optional<std::string> RedisClient::get(
    const std::string& key,
    std::string* error_message) {
    if (!is_connected()) {
        if (error_message != nullptr) {
            *error_message = "redis client is not connected";
        }
        return std::nullopt;
    }
    auto* raw_reply = static_cast<redisReply*>(
        redisCommand(context_, "GET %b", key.data(), key.size()));
    ReplyGuard reply(raw_reply);
    if (!reply_ok(reply.reply, error_message) || reply.reply->type == REDIS_REPLY_NIL) {
        return std::nullopt;
    }
    if (reply.reply->type != REDIS_REPLY_STRING) {
        if (error_message != nullptr) {
            *error_message = "redis GET did not return a string";
        }
        return std::nullopt;
    }
    return std::string(reply.reply->str, static_cast<std::size_t>(reply.reply->len));
}

std::optional<std::string> RedisClient::get_and_remove(
    const std::string& key,
    std::string* error_message) {
    if (!is_connected()) {
        if (error_message != nullptr) {
            *error_message = "redis client is not connected";
        }
        return std::nullopt;
    }
    auto* raw_reply = static_cast<redisReply*>(
        redisCommand(context_, "GETDEL %b", key.data(), key.size()));
    ReplyGuard reply(raw_reply);
    if (!reply_ok(reply.reply, error_message) || reply.reply->type == REDIS_REPLY_NIL) {
        return std::nullopt;
    }
    if (reply.reply->type != REDIS_REPLY_STRING) {
        if (error_message != nullptr) {
            *error_message = "redis GETDEL did not return a string";
        }
        return std::nullopt;
    }
    return std::string(reply.reply->str, static_cast<std::size_t>(reply.reply->len));
}

bool RedisClient::remove(const std::string& key, std::string* error_message) {
    if (!is_connected()) {
        if (error_message != nullptr) {
            *error_message = "redis client is not connected";
        }
        return false;
    }
    auto* raw_reply = static_cast<redisReply*>(
        redisCommand(context_, "DEL %b", key.data(), key.size()));
    ReplyGuard reply(raw_reply);
    return reply_ok(reply.reply, error_message);
}

bool RedisClient::expire(const std::string& key, int seconds, std::string* error_message) {
    if (!is_connected()) {
        if (error_message != nullptr) {
            *error_message = "redis client is not connected";
        }
        return false;
    }
    auto* raw_reply = static_cast<redisReply*>(
        redisCommand(context_, "EXPIRE %b %d", key.data(), key.size(), seconds));
    ReplyGuard reply(raw_reply);
    return reply_ok(reply.reply, error_message);
}

bool RedisClient::is_connected() const {
    return context_ != nullptr && context_->err == 0;
}

void RedisClient::close() {
    if (context_ != nullptr) {
        redisFree(context_);
        context_ = nullptr;
    }
}

}  // namespace mmo::runtime::storage

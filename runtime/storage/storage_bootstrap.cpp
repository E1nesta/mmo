#include "runtime/storage/storage_bootstrap.h"

#include <cstdlib>
#include <stdexcept>

namespace runtime::storage {
namespace {

std::string optional_env_value(const std::string& name) {
    if (name.empty()) {
        return {};
    }
    const char* value = std::getenv(name.c_str());
    return value == nullptr ? std::string{} : std::string(value);
}

std::string required_env_value(
    const std::string& name,
    const std::string& field_name,
    const std::string& default_value = {}) {
    const auto value = optional_env_value(name);
    if (value.empty() && !default_value.empty()) {
        return default_value;
    }
    if (value.empty()) {
        throw std::runtime_error(
            "missing required environment variable for " + field_name + ": " +
            name);
    }
    return value;
}

}  // namespace

MysqlConfig make_mysql_config_with_environment(
    const runtime::foundation::MysqlConfig& config,
    const std::string& environment) {
    MysqlConfig output;
    output.host = config.host;
    output.port = config.port;
    output.database = config.database;
    output.user = config.user;
    const std::string local_default =
        environment == "local" ? "mmo_dev_local" : std::string{};
    output.password = required_env_value(
        config.password_env, "storage.mysql.password", local_default);
    return output;
}

MysqlConfig make_mysql_config(
    const runtime::foundation::MysqlConfig& config) {
    return make_mysql_config_with_environment(config, {});
}

RedisConfig make_redis_config(
    const runtime::foundation::RedisConfig& config) {
    RedisConfig output;
    output.host = config.host;
    output.port = config.port;
    output.database = config.database;
    output.password = optional_env_value(config.password_env);
    return output;
}

bool initialize_mysql_pool(
    const runtime::foundation::ServerConfig& config,
    std::shared_ptr<MysqlConnectionPool>* pool,
    std::string* error_message) {
    if (pool == nullptr) {
        if (error_message != nullptr) {
            *error_message = "mysql pool output is null";
        }
        return false;
    }
    try {
        auto next_pool = std::make_shared<MysqlConnectionPool>(
            make_mysql_config_with_environment(
                config.storage.mysql, config.environment),
            static_cast<std::size_t>(config.storage.mysql.pool_size));
        if (!next_pool->initialize(error_message)) {
            return false;
        }
        *pool = std::move(next_pool);
        return true;
    } catch (const std::exception& error) {
        if (error_message != nullptr) {
            *error_message = error.what();
        }
        return false;
    }
}

bool initialize_redis_pool(
    const runtime::foundation::ServerConfig& config,
    std::shared_ptr<RedisConnectionPool>* pool,
    std::string* error_message) {
    if (pool == nullptr) {
        if (error_message != nullptr) {
            *error_message = "redis pool output is null";
        }
        return false;
    }
    try {
        auto next_pool = std::make_shared<RedisConnectionPool>(
            make_redis_config(config.storage.redis),
            static_cast<std::size_t>(config.storage.redis.pool_size));
        if (!next_pool->initialize(error_message)) {
            return false;
        }
        *pool = std::move(next_pool);
        return true;
    } catch (const std::exception& error) {
        if (error_message != nullptr) {
            *error_message = error.what();
        }
        return false;
    }
}

}  // namespace runtime::storage

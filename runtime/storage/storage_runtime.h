#pragma once

#include <memory>
#include <string>

#include "runtime/foundation/server_config.h"
#include "runtime/storage/mysql_connection_pool.h"
#include "runtime/storage/redis_connection_pool.h"

namespace runtime::storage {

MysqlConfig make_mysql_config(
    const runtime::foundation::MysqlConfig& config);
RedisConfig make_redis_config(
    const runtime::foundation::RedisConfig& config);

bool initialize_mysql_pool(
    const runtime::foundation::ServerConfig& config,
    std::shared_ptr<MysqlConnectionPool>* pool,
    std::string* error_message);

bool initialize_redis_pool(
    const runtime::foundation::ServerConfig& config,
    std::shared_ptr<RedisConnectionPool>* pool,
    std::string* error_message);

}  // namespace runtime::storage

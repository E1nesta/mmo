#pragma once

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "runtime/storage/redis_client.h"

namespace mmo::runtime::storage {

class RedisConnectionPool {
public:
    using ClientLease = std::shared_ptr<RedisClient>;

    RedisConnectionPool(RedisConfig config, std::size_t pool_size);
    RedisConnectionPool(const RedisConnectionPool&) = delete;
    RedisConnectionPool& operator=(const RedisConnectionPool&) = delete;
    ~RedisConnectionPool();

    bool initialize(std::string* error_message);
    ClientLease acquire();
    std::size_t size() const;
    std::size_t available_count() const;
    void close();

private:
    void release(RedisClient* client);

    RedisConfig config_;
    std::size_t pool_size_{};
    std::vector<std::unique_ptr<RedisClient>> clients_;
    std::queue<RedisClient*> available_;
    mutable std::mutex mutex_;
    std::condition_variable ready_;
    bool initialized_{};
    bool closing_{};
};

}  // namespace mmo::runtime::storage

#include "runtime/storage/redis_connection_pool.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace runtime::storage {

RedisConnectionPool::RedisConnectionPool(RedisConfig config, std::size_t pool_size)
    : config_(std::move(config)), pool_size_(std::max<std::size_t>(1, pool_size)) {}

RedisConnectionPool::~RedisConnectionPool() {
    close();
}

bool RedisConnectionPool::initialize(std::string* error_message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return true;
    }

    clients_.reserve(pool_size_);
    for (std::size_t index = 0; index < pool_size_; ++index) {
        auto client = std::make_unique<RedisClient>();
        if (!client->connect(config_, error_message)) {
            clients_.clear();
            while (!available_.empty()) {
                available_.pop();
            }
            return false;
        }
        available_.push(client.get());
        clients_.push_back(std::move(client));
    }
    initialized_ = true;
    return true;
}

RedisConnectionPool::ClientLease RedisConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!initialized_) {
        throw std::runtime_error("redis connection pool is not initialized");
    }
    ready_.wait(lock, [this]() { return closing_ || !available_.empty(); });
    if (closing_) {
        throw std::runtime_error("redis connection pool is closing");
    }

    auto* client = available_.front();
    available_.pop();
    return ClientLease(client, [this](RedisClient* value) { release(value); });
}

std::size_t RedisConnectionPool::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return clients_.size();
}

std::size_t RedisConnectionPool::available_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return available_.size();
}

void RedisConnectionPool::close() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closing_) {
            return;
        }
        closing_ = true;
        while (!available_.empty()) {
            available_.pop();
        }
        for (auto& client : clients_) {
            client->close();
        }
        initialized_ = false;
    }
    ready_.notify_all();
}

void RedisConnectionPool::release(RedisClient* client) {
    if (client == nullptr) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closing_) {
            return;
        }
        available_.push(client);
    }
    ready_.notify_one();
}

}  // namespace runtime::storage

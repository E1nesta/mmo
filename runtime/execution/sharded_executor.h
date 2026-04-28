#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace mmo::runtime::execution {

class ShardedExecutor {
public:
    using Task = std::function<void()>;

    explicit ShardedExecutor(std::size_t shard_count);
    ShardedExecutor(const ShardedExecutor&) = delete;
    ShardedExecutor& operator=(const ShardedExecutor&) = delete;
    ~ShardedExecutor();

    void post(std::uint64_t shard_key, Task task);
    std::size_t shard_count() const;
    void stop();

private:
    struct Shard;

    void run_shard(Shard& shard);

    std::vector<std::unique_ptr<Shard>> shards_;
};

}  // namespace mmo::runtime::execution

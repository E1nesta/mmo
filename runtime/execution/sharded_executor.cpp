#include "runtime/execution/sharded_executor.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace mmo::runtime::execution {

struct ShardedExecutor::Shard {
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<Task> tasks;
    std::thread worker;
    bool stopping{};
};

ShardedExecutor::ShardedExecutor(std::size_t shard_count) {
    if (shard_count == 0) {
        throw std::invalid_argument("shard_count must be greater than zero");
    }

    shards_.reserve(shard_count);
    for (std::size_t index = 0; index < shard_count; ++index) {
        auto shard = std::make_unique<Shard>();
        auto* shard_ptr = shard.get();
        shard->worker = std::thread([this, shard_ptr]() { run_shard(*shard_ptr); });
        shards_.push_back(std::move(shard));
    }
}

ShardedExecutor::~ShardedExecutor() {
    stop();
}

void ShardedExecutor::post(std::uint64_t shard_key, Task task) {
    auto& shard = *shards_[shard_key % shards_.size()];
    {
        std::lock_guard<std::mutex> lock(shard.mutex);
        shard.tasks.push_back(std::move(task));
    }
    shard.ready.notify_one();
}

std::size_t ShardedExecutor::shard_count() const {
    return shards_.size();
}

void ShardedExecutor::stop() {
    for (auto& shard : shards_) {
        {
            std::lock_guard<std::mutex> lock(shard->mutex);
            shard->stopping = true;
        }
        shard->ready.notify_one();
    }

    for (auto& shard : shards_) {
        if (shard->worker.joinable()) {
            shard->worker.join();
        }
    }
}

void ShardedExecutor::run_shard(Shard& shard) {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(shard.mutex);
            shard.ready.wait(lock, [&shard]() {
                return shard.stopping || !shard.tasks.empty();
            });
            if (shard.stopping && shard.tasks.empty()) {
                return;
            }

            task = std::move(shard.tasks.front());
            shard.tasks.pop_front();
        }
        task();
    }
}

}  // namespace mmo::runtime::execution

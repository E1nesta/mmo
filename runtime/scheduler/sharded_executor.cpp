#include "runtime/scheduler/sharded_executor.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace runtime::scheduler {

struct ShardedExecutor::Shard {
    std::mutex mutex;
    std::condition_variable ready;
    std::deque<Task> tasks;
    std::thread worker;
    bool stopping{};
};

bool PostResult::accepted() const {
    return status == PostStatus::kAccepted;
}

ShardedExecutor::ShardedExecutor(std::size_t shard_count)
    : ShardedExecutor(ShardedExecutorOptions{
          shard_count,
          kDefaultMaxQueueDepthPerShard}) {}

ShardedExecutor::ShardedExecutor(ShardedExecutorOptions options)
    : max_queue_depth_per_shard_(options.max_queue_depth_per_shard) {
    if (options.shard_count == 0) {
        throw std::invalid_argument("shard_count must be greater than zero");
    }
    if (options.max_queue_depth_per_shard == 0) {
        throw std::invalid_argument(
            "max_queue_depth_per_shard must be greater than zero");
    }

    shards_.reserve(options.shard_count);
    for (std::size_t index = 0; index < options.shard_count; ++index) {
        auto shard = std::make_unique<Shard>();
        auto* shard_ptr = shard.get();
        shard->worker = std::thread([this, shard_ptr]() { run_shard(*shard_ptr); });
        shards_.push_back(std::move(shard));
    }
}

ShardedExecutor::~ShardedExecutor() {
    stop();
}

PostResult ShardedExecutor::post(std::uint64_t shard_key, Task task) {
    if (!task || stopped_.load(std::memory_order_acquire)) {
        return PostResult{PostStatus::kStopped};
    }

    auto& shard = *shards_[shard_key % shards_.size()];
    {
        std::lock_guard<std::mutex> lock(shard.mutex);
        if (shard.stopping) {
            return PostResult{PostStatus::kStopped};
        }
        if (shard.tasks.size() >= max_queue_depth_per_shard_) {
            return PostResult{PostStatus::kQueueFull};
        }
        shard.tasks.push_back(std::move(task));
        queued_task_count_.fetch_add(1, std::memory_order_relaxed);
    }
    shard.ready.notify_one();
    return PostResult{PostStatus::kAccepted};
}

std::size_t ShardedExecutor::shard_count() const {
    return shards_.size();
}

std::size_t ShardedExecutor::queued_task_count() const {
    return queued_task_count_.load(std::memory_order_relaxed);
}

bool ShardedExecutor::is_stopped() const {
    return stopped_.load(std::memory_order_acquire);
}

void ShardedExecutor::stop() {
    bool expected = false;
    if (!stopped_.compare_exchange_strong(expected, true)) {
        return;
    }

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
            queued_task_count_.fetch_sub(1, std::memory_order_relaxed);
        }
        task();
    }
}

}  // namespace runtime::scheduler

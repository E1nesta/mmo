#pragma once

#include <condition_variable>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace runtime::scheduler {

inline constexpr std::size_t kDefaultMaxQueueDepthPerShard = 1024;

enum class PostStatus {
    kAccepted,
    kStopped,
    kQueueFull,
};

struct PostResult {
    PostStatus status{PostStatus::kAccepted};

    bool accepted() const;
};

struct ShardedExecutorOptions {
    std::size_t shard_count{};
    std::size_t max_queue_depth_per_shard{kDefaultMaxQueueDepthPerShard};
};

struct ShardedExecutorStats {
    std::size_t shard_count{};
    std::size_t max_queue_depth_per_shard{};
    std::size_t queued_task_count{};
    std::uint64_t accepted_task_count{};
    std::uint64_t rejected_task_count{};
    std::uint64_t completed_task_count{};
    std::uint64_t failed_task_count{};
};

class ShardedExecutor {
public:
    using Task = std::function<void()>;

    explicit ShardedExecutor(std::size_t shard_count);
    explicit ShardedExecutor(ShardedExecutorOptions options);
    ShardedExecutor(const ShardedExecutor&) = delete;
    ShardedExecutor& operator=(const ShardedExecutor&) = delete;
    ~ShardedExecutor();

    PostResult post(std::uint64_t shard_key, Task task);
    std::size_t shard_count() const;
    std::size_t queued_task_count() const;
    ShardedExecutorStats stats() const;
    bool is_stopped() const;
    void stop();

private:
    struct Shard;

    void run_shard(Shard& shard);

    std::vector<std::unique_ptr<Shard>> shards_;
    std::size_t max_queue_depth_per_shard_{kDefaultMaxQueueDepthPerShard};
    std::atomic<std::size_t> queued_task_count_{};
    std::atomic<std::uint64_t> accepted_task_count_{};
    std::atomic<std::uint64_t> rejected_task_count_{};
    std::atomic<std::uint64_t> completed_task_count_{};
    std::atomic<std::uint64_t> failed_task_count_{};
    std::atomic<bool> stopped_{};
};

}  // namespace runtime::scheduler

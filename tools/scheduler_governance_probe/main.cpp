#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

#include "runtime/scheduler/sharded_executor.h"

namespace {

void verify_rejects_invalid_options() {
    bool zero_shards_rejected = false;
    try {
        runtime::scheduler::ShardedExecutor executor(
            runtime::scheduler::ShardedExecutorOptions{0, 1});
    } catch (const std::invalid_argument&) {
        zero_shards_rejected = true;
    }
    assert(zero_shards_rejected);

    bool zero_depth_rejected = false;
    try {
        runtime::scheduler::ShardedExecutor executor(
            runtime::scheduler::ShardedExecutorOptions{1, 0});
    } catch (const std::invalid_argument&) {
        zero_depth_rejected = true;
    }
    assert(zero_depth_rejected);
}

void verify_queue_full_and_stop() {
    runtime::scheduler::ShardedExecutor executor(
        runtime::scheduler::ShardedExecutorOptions{1, 1});

    std::mutex mutex;
    std::condition_variable cv;
    bool first_started = false;
    bool unblock_first = false;

    const auto first = executor.post(1, [&]() {
        std::unique_lock<std::mutex> lock(mutex);
        first_started = true;
        cv.notify_one();
        cv.wait(lock, [&]() { return unblock_first; });
    });
    assert(first.accepted());

    {
        std::unique_lock<std::mutex> lock(mutex);
        assert(cv.wait_for(lock, std::chrono::seconds(1), [&]() {
            return first_started;
        }));
    }

    const auto second = executor.post(1, []() {});
    assert(second.accepted());

    const auto third = executor.post(1, []() {});
    assert(third.status == runtime::scheduler::PostStatus::kQueueFull);

    {
        std::lock_guard<std::mutex> lock(mutex);
        unblock_first = true;
    }
    cv.notify_one();
    executor.stop();

    const auto after_stop = executor.post(1, []() {});
    assert(after_stop.status == runtime::scheduler::PostStatus::kStopped);

    const auto stats = executor.stats();
    assert(stats.shard_count == 1);
    assert(stats.max_queue_depth_per_shard == 1);
    assert(stats.accepted_task_count == 2);
    assert(stats.completed_task_count == 2);
    assert(stats.rejected_task_count == 2);
    assert(executor.is_stopped());
}

}  // namespace

int main() {
    verify_rejects_invalid_options();
    verify_queue_full_and_stop();
    return 0;
}

// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/execution/execution_types.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace framework::execution {

enum class SubmitFailureCode {
    kNone,
    kExecutionKeyUnresolved,
    kNotStarted,
    kStopping,
    kQueueLimitExceeded,
};

// 固定分片执行业务任务，降低并发冲突。
// 传输线程将任务移交到分片执行器，相同执行键始终落同一分片。
class ShardedRequestExecutor {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    struct Options {
        std::size_t worker_threads = 1;
        std::size_t shard_count = 1;
        std::size_t queue_limit = 1024;
    };

    using Task = std::function<void()>;

    explicit ShardedRequestExecutor(Options options);
    ~ShardedRequestExecutor();

    ShardedRequestExecutor(const ShardedRequestExecutor&) = delete;
    ShardedRequestExecutor& operator=(const ShardedRequestExecutor&) = delete;

    bool Start(std::string* error_message = nullptr);
    bool Submit(const ExecutionKey& key,
                Task task,
                std::size_t* shard_index = nullptr,
                std::string* error_message = nullptr,
                SubmitFailureCode* failure_code = nullptr);
    [[nodiscard]] std::optional<std::size_t> PreviewShard(const ExecutionKey& key) const;
    // 停止接收新任务，同时允许已入队任务继续排空。
    void StopAccepting();
    // 等待队列与运行中任务收敛完成，或直到超时。
    [[nodiscard]] bool WaitForDrain(std::chrono::milliseconds timeout);
    // 由调用方决定排空或超时策略后，再关闭工作线程。
    void Shutdown();

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    struct Shard {
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<Task> tasks;
        std::thread worker;
        bool stop = false;
    };

    void RunShard(Shard* shard);
    [[nodiscard]] std::size_t ResolveShard(const ExecutionKey& key) const;
    bool TryReserveQueueSlot(std::string* error_message, SubmitFailureCode* failure_code);
    void ReleaseQueueSlot();

    Options options_;
    std::vector<std::unique_ptr<Shard>> shards_;
    std::atomic_bool accepting_{true};
    std::atomic_bool started_{false};
    std::atomic<std::size_t> pending_tasks_{0};
    mutable std::mutex drain_mutex_;
    std::condition_variable drain_cv_;
};

}  // namespace framework::execution

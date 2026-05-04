#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

namespace runtime::execution {

class IOContextPool {
public:
    explicit IOContextPool(std::size_t thread_count);
    IOContextPool(const IOContextPool&) = delete;
    IOContextPool& operator=(const IOContextPool&) = delete;
    ~IOContextPool();

    boost::asio::io_context& next();
    std::size_t size() const;

    void start();
    void run();
    void stop();
    void join();

private:
    using WorkGuard =
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    std::vector<std::unique_ptr<boost::asio::io_context>> contexts_;
    std::vector<std::unique_ptr<WorkGuard>> work_guards_;
    std::vector<std::thread> threads_;
    std::atomic<std::size_t> next_index_{};
    std::atomic<bool> started_{};
};

}  // namespace runtime::execution

#include "runtime/scheduler/io_context_pool.h"

#include <stdexcept>

namespace runtime::scheduler {
namespace {

void run_io_context(boost::asio::io_context& context) {
    while (!context.stopped()) {
        try {
            context.run();
            return;
        } catch (...) {
        }
    }
}

}  // namespace

IOContextPool::IOContextPool(std::size_t thread_count) {
    if (thread_count == 0) {
        throw std::invalid_argument("io context thread count must be greater than zero");
    }

    contexts_.reserve(thread_count);
    work_guards_.reserve(thread_count);
    for (std::size_t index = 0; index < thread_count; ++index) {
        auto context = std::make_unique<boost::asio::io_context>();
        work_guards_.push_back(
            std::make_unique<WorkGuard>(boost::asio::make_work_guard(*context)));
        contexts_.push_back(std::move(context));
    }
}

IOContextPool::~IOContextPool() {
    stop();
    join();
}

boost::asio::io_context& IOContextPool::next() {
    const auto index = next_index_.fetch_add(1, std::memory_order_relaxed);
    return *contexts_[index % contexts_.size()];
}

std::size_t IOContextPool::size() const {
    return contexts_.size();
}

void IOContextPool::start() {
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true)) {
        return;
    }

    threads_.reserve(contexts_.size());
    for (auto& context : contexts_) {
        auto* raw_context = context.get();
        threads_.emplace_back([raw_context]() { run_io_context(*raw_context); });
    }
}

void IOContextPool::run() {
    start();
    join();
}

void IOContextPool::stop() {
    for (auto& guard : work_guards_) {
        guard.reset();
    }
    for (auto& context : contexts_) {
        context->stop();
    }
}

void IOContextPool::join() {
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

}  // namespace runtime::scheduler

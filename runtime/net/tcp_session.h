#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include <boost/asio/ip/tcp.hpp>

#include "runtime/net/frame_transport.h"

namespace runtime::net {

struct TcpSessionOptions {
    std::uint32_t max_write_queue_depth{kDefaultMaxWriteQueueDepth};
    std::shared_ptr<TransportCounters> counters;
    std::function<void(TcpCloseReason)> on_close;
};

class TcpSession : public std::enable_shared_from_this<TcpSession> {
public:
    using ReadFrameHandler = std::function<void(
        bool ok,
        std::vector<std::uint8_t> frame)>;
    using WriteFrameHandler = std::function<void(bool ok)>;

    explicit TcpSession(
        boost::asio::ip::tcp::socket socket,
        TcpSessionOptions options = {});
    ~TcpSession();

    TcpSession(const TcpSession&) = delete;
    TcpSession& operator=(const TcpSession&) = delete;
    TcpSession(TcpSession&& other) = delete;
    TcpSession& operator=(TcpSession&&) = delete;

    bool read_frame(std::uint32_t max_frame_bytes, std::vector<std::uint8_t>* frame);
    bool write_frame(const std::vector<std::uint8_t>& frame);
    void async_read_frame(
        std::uint32_t max_frame_bytes,
        ReadFrameHandler handler);
    void async_write_frame(
        std::vector<std::uint8_t> frame,
        WriteFrameHandler handler);
    void close(TcpCloseReason reason = TcpCloseReason::kHandlerStopped);
    void async_close(TcpCloseReason reason = TcpCloseReason::kHandlerStopped);
    TcpCloseReason close_reason() const;
    boost::asio::ip::tcp::socket::executor_type executor();

private:
    struct PendingWrite {
        std::shared_ptr<std::vector<std::uint8_t>> frame;
        WriteFrameHandler handler;
    };

    bool flush_write_queue();
    void start_async_write();

    boost::asio::ip::tcp::socket socket_;
    TcpSessionOptions options_;
    mutable std::mutex mutex_;
    std::deque<PendingWrite> write_queue_;
    bool async_write_in_progress_{false};
    TcpCloseReason close_reason_{TcpCloseReason::kNone};
};

}  // namespace runtime::net

#include "runtime/net/tcp_session.h"

#include <array>
#include <atomic>
#include <memory>
#include <utility>

#include <boost/asio/read.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

namespace runtime::net {
namespace {

std::uint32_t decode_u32(const std::array<std::uint8_t, 4>& bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
           (static_cast<std::uint32_t>(bytes[1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[2]) << 8U) |
           static_cast<std::uint32_t>(bytes[3]);
}

}  // namespace

TcpSession::TcpSession(
    boost::asio::ip::tcp::socket socket,
    TcpSessionOptions options)
    : socket_(std::move(socket)),
      options_(options) {
    if (options_.max_write_queue_depth == 0) {
        options_.max_write_queue_depth = 1;
    }
}

TcpSession::~TcpSession() {
    close(TcpCloseReason::kHandlerStopped);
}

bool TcpSession::read_frame(
    std::uint32_t max_frame_bytes,
    std::vector<std::uint8_t>* frame) {
    if (frame == nullptr) {
        return false;
    }
    frame->clear();

    std::array<std::uint8_t, 4> header{};
    boost::system::error_code error;
    boost::asio::read(socket_, boost::asio::buffer(header), error);
    if (error) {
        if (options_.counters != nullptr) {
            options_.counters->errors.fetch_add(1, std::memory_order_relaxed);
        }
        close(error == boost::asio::error::eof
                  ? TcpCloseReason::kPeerClosed
                  : TcpCloseReason::kReadFailed);
        return false;
    }

    const auto body_size = decode_u32(header);
    if (body_size > max_frame_bytes) {
        if (options_.counters != nullptr) {
            options_.counters->errors.fetch_add(1, std::memory_order_relaxed);
        }
        close(TcpCloseReason::kFrameTooLarge);
        return false;
    }

    frame->reserve(header.size() + body_size);
    frame->insert(frame->end(), header.begin(), header.end());
    const auto body_offset = frame->size();
    frame->resize(body_offset + body_size);
    if (body_size == 0) {
        if (options_.counters != nullptr) {
            options_.counters->read_frames.fetch_add(
                1,
                std::memory_order_relaxed);
            options_.counters->read_bytes.fetch_add(
                frame->size(),
                std::memory_order_relaxed);
        }
        return true;
    }

    boost::asio::read(
        socket_,
        boost::asio::buffer(frame->data() + body_offset, body_size),
        error);
    if (error) {
        if (options_.counters != nullptr) {
            options_.counters->errors.fetch_add(1, std::memory_order_relaxed);
        }
        close(error == boost::asio::error::eof
                  ? TcpCloseReason::kPeerClosed
                  : TcpCloseReason::kReadFailed);
        return false;
    }
    if (options_.counters != nullptr) {
        options_.counters->read_frames.fetch_add(1, std::memory_order_relaxed);
        options_.counters->read_bytes.fetch_add(
            frame->size(),
            std::memory_order_relaxed);
    }
    return !error;
}

bool TcpSession::write_frame(const std::vector<std::uint8_t>& frame) {
    if (frame.empty()) {
        return false;
    }
    bool queue_full = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (close_reason_ != TcpCloseReason::kNone) {
            return false;
        }
        if (write_queue_.size() >= options_.max_write_queue_depth) {
            queue_full = true;
            if (options_.counters != nullptr) {
                options_.counters->errors.fetch_add(
                    1,
                    std::memory_order_relaxed);
            }
        } else {
            write_queue_.push_back(PendingWrite{
                std::make_shared<std::vector<std::uint8_t>>(frame),
                {}});
        }
    }
    if (queue_full) {
        close(TcpCloseReason::kWriteQueueFull);
        return false;
    }
    return flush_write_queue();
}

void TcpSession::async_read_frame(
    std::uint32_t max_frame_bytes,
    ReadFrameHandler handler) {
    if (!handler) {
        return;
    }
    bool already_closed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (close_reason_ != TcpCloseReason::kNone) {
            already_closed = true;
        }
    }
    if (already_closed) {
        handler(false, {});
        return;
    }
    auto self = shared_from_this();
    auto header = std::make_shared<std::array<std::uint8_t, 4>>();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(*header),
        [self,
         header,
         max_frame_bytes,
         handler = std::move(handler)](
            const boost::system::error_code& error,
            std::size_t) mutable {
            if (error) {
                if (self->options_.counters != nullptr) {
                    self->options_.counters->errors.fetch_add(
                        1,
                        std::memory_order_relaxed);
                }
                self->close(error == boost::asio::error::eof
                                ? TcpCloseReason::kPeerClosed
                                : TcpCloseReason::kReadFailed);
                handler(false, {});
                return;
            }

            const auto body_size = decode_u32(*header);
            if (body_size > max_frame_bytes) {
                if (self->options_.counters != nullptr) {
                    self->options_.counters->errors.fetch_add(
                        1,
                        std::memory_order_relaxed);
                }
                self->close(TcpCloseReason::kFrameTooLarge);
                handler(false, {});
                return;
            }

            auto frame = std::make_shared<std::vector<std::uint8_t>>();
            frame->reserve(header->size() + body_size);
            frame->insert(frame->end(), header->begin(), header->end());
            const auto body_offset = frame->size();
            frame->resize(body_offset + body_size);
            if (body_size == 0) {
                if (self->options_.counters != nullptr) {
                    self->options_.counters->read_frames.fetch_add(
                        1,
                        std::memory_order_relaxed);
                    self->options_.counters->read_bytes.fetch_add(
                        frame->size(),
                        std::memory_order_relaxed);
                }
                handler(true, std::move(*frame));
                return;
            }

            boost::asio::async_read(
                self->socket_,
                boost::asio::buffer(frame->data() + body_offset, body_size),
                [self,
                 frame,
                 handler = std::move(handler)](
                    const boost::system::error_code& body_error,
                    std::size_t) mutable {
                    if (body_error) {
                        if (self->options_.counters != nullptr) {
                            self->options_.counters->errors.fetch_add(
                                1,
                                std::memory_order_relaxed);
                        }
                        self->close(body_error == boost::asio::error::eof
                                        ? TcpCloseReason::kPeerClosed
                                        : TcpCloseReason::kReadFailed);
                        handler(false, {});
                        return;
                    }
                    if (self->options_.counters != nullptr) {
                        self->options_.counters->read_frames.fetch_add(
                            1,
                            std::memory_order_relaxed);
                        self->options_.counters->read_bytes.fetch_add(
                            frame->size(),
                            std::memory_order_relaxed);
                    }
                    handler(true, std::move(*frame));
                });
        });
}

void TcpSession::async_write_frame(
    std::vector<std::uint8_t> frame,
    WriteFrameHandler handler) {
    if (frame.empty()) {
        if (handler) {
            handler(false);
        }
        return;
    }

    bool queue_full = false;
    bool already_closed = false;
    bool should_start_write = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (close_reason_ != TcpCloseReason::kNone) {
            already_closed = true;
        } else if (write_queue_.size() >= options_.max_write_queue_depth) {
            queue_full = true;
        } else {
            write_queue_.push_back(PendingWrite{
                std::make_shared<std::vector<std::uint8_t>>(std::move(frame)),
                std::move(handler)});
            if (!async_write_in_progress_) {
                async_write_in_progress_ = true;
                should_start_write = true;
            }
        }
    }

    if (already_closed) {
        if (handler) {
            handler(false);
        }
        return;
    }
    if (queue_full) {
        if (options_.counters != nullptr) {
            options_.counters->errors.fetch_add(1, std::memory_order_relaxed);
        }
        async_close(TcpCloseReason::kWriteQueueFull);
        if (handler) {
            handler(false);
        }
        return;
    }
    if (should_start_write) {
        start_async_write();
    }
}

void TcpSession::close(TcpCloseReason reason) {
    std::function<void(TcpCloseReason)> on_close;
    bool should_notify = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (close_reason_ == TcpCloseReason::kNone) {
            close_reason_ = reason;
            on_close = options_.on_close;
            should_notify = true;
        }
        write_queue_.clear();
        async_write_in_progress_ = false;
    }
    boost::system::error_code ignored;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
    if (should_notify && on_close) {
        on_close(reason);
    }
}

void TcpSession::async_close(TcpCloseReason reason) {
    auto self = shared_from_this();
    boost::asio::post(
        socket_.get_executor(),
        [self, reason]() {
            self->close(reason);
        });
}

TcpCloseReason TcpSession::close_reason() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return close_reason_;
}

boost::asio::ip::tcp::socket::executor_type TcpSession::executor() {
    return socket_.get_executor();
}

bool TcpSession::flush_write_queue() {
    while (true) {
        PendingWrite pending_write;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (write_queue_.empty()) {
                return true;
            }
            pending_write = std::move(write_queue_.front());
            write_queue_.pop_front();
        }

        if (pending_write.frame == nullptr || pending_write.frame->empty()) {
            continue;
        }

        boost::system::error_code error;
        boost::asio::write(
            socket_,
            boost::asio::buffer(*pending_write.frame),
            error);
        if (error) {
            if (options_.counters != nullptr) {
                options_.counters->errors.fetch_add(
                    1,
                    std::memory_order_relaxed);
            }
            close(TcpCloseReason::kWriteFailed);
            return false;
        }
        if (options_.counters != nullptr) {
            options_.counters->written_frames.fetch_add(
                1,
                std::memory_order_relaxed);
            options_.counters->written_bytes.fetch_add(
                pending_write.frame->size(),
                std::memory_order_relaxed);
        }
    }
}

void TcpSession::start_async_write() {
    auto self = shared_from_this();
    std::shared_ptr<std::vector<std::uint8_t>> frame;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (write_queue_.empty()) {
            async_write_in_progress_ = false;
            return;
        }
        frame = write_queue_.front().frame;
    }

    boost::asio::async_write(
        socket_,
        boost::asio::buffer(*frame),
        [self, frame](
            const boost::system::error_code& error,
            std::size_t) mutable {
            WriteFrameHandler handler;
            {
                std::lock_guard<std::mutex> lock(self->mutex_);
                if (!self->write_queue_.empty()) {
                    handler = std::move(self->write_queue_.front().handler);
                    self->write_queue_.pop_front();
                }
                self->async_write_in_progress_ = false;
            }

            if (error) {
                if (self->options_.counters != nullptr) {
                    self->options_.counters->errors.fetch_add(
                        1,
                        std::memory_order_relaxed);
                }
                self->close(TcpCloseReason::kWriteFailed);
                if (handler) {
                    handler(false);
                }
                return;
            }

            if (self->options_.counters != nullptr) {
                self->options_.counters->written_frames.fetch_add(
                    1,
                    std::memory_order_relaxed);
                self->options_.counters->written_bytes.fetch_add(
                    frame->size(),
                    std::memory_order_relaxed);
            }
            if (handler) {
                handler(true);
            }
            self->start_async_write();
        });
}

}  // namespace runtime::net

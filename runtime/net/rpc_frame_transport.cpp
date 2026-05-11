#include "runtime/net/rpc_frame_transport.h"

#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <boost/asio/post.hpp>

#include "runtime/net/rpc_frame_codec.h"
#include "runtime/net/tcp_listener.h"
#include "runtime/protocol/frame.h"
#include "runtime/protocol/payload_utils.h"
#include "runtime/scheduler/sharded_executor.h"

namespace runtime::net {
namespace {

std::uint32_t max_rpc_frame_bytes(const TransportOptions& options) {
    return options.max_payload_bytes + RpcFrameCodec::kRpcFrameOverheadBytes;
}

class RpcFrameSession final
    : public std::enable_shared_from_this<RpcFrameSession> {
public:
    RpcFrameSession(
        std::shared_ptr<TcpSession> session,
        RpcFrameHandler handler,
        std::shared_ptr<runtime::scheduler::ShardedExecutor> handler_executor,
        TransportOptions options)
        : session_(std::move(session)),
          handler_(std::move(handler)),
          handler_executor_(std::move(handler_executor)),
          options_(options) {}

    void start() {
        maybe_start_read();
    }

private:
    void maybe_start_read() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_ || read_in_progress_ ||
                in_flight_frames_ >=
                    options_.max_inflight_frames_per_connection) {
                return;
            }
            read_in_progress_ = true;
        }
        auto self = shared_from_this();
        session_->async_read_frame(
            max_rpc_frame_bytes(options_),
            [self](bool ok, std::vector<std::uint8_t> data) mutable {
                self->on_read(ok, std::move(data));
            });
    }

    void on_read(bool ok, std::vector<std::uint8_t> data) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            read_in_progress_ = false;
        }
        if (!ok) {
            mark_closed();
            return;
        }

        std::string error_message;
        runtime::protocol::FrameMessage request;
        if (!RpcFrameCodec::decode_rpc_frame(
                data,
                options_.max_payload_bytes,
                &request,
                &error_message)) {
            mark_closed();
            session_->close(TcpCloseReason::kDecodeFailed);
            return;
        }

        add_inflight_frame();
        const auto shard_key = request.route_key() != 0
            ? request.route_key()
            : request.request_id();
        auto self = shared_from_this();
        const auto post_result = handler_executor_->post(
            shard_key,
            [self, request]() {
                self->handle_request(request);
            });
        if (!post_result.accepted()) {
            if (runtime::protocol::is_cast_like(request.mode())) {
                mark_closed();
                finish_inflight_frame();
                session_->close(TcpCloseReason::kHandlerQueueFull);
                return;
            }
            auto response = runtime::protocol::make_error_frame(
                request,
                503,
                "rpc handler queue is full");
            auto encoded = RpcFrameCodec::encode_rpc_frame(
                response,
                options_.max_payload_bytes,
                &error_message);
            if (encoded.empty()) {
                mark_closed();
                finish_inflight_frame();
                session_->close(TcpCloseReason::kEncodeFailed);
                return;
            }
            session_->async_write_frame(
                std::move(encoded),
                [self](bool write_ok) {
                    if (!write_ok) {
                        self->mark_closed();
                        self->finish_inflight_frame();
                        return;
                    }
                    self->finish_inflight_frame();
                });
            return;
        }
        maybe_start_read();
    }

    void handle_request(const runtime::protocol::FrameMessage& request) {
        auto self = shared_from_this();
        try {
            handler_(
                request,
                [self, request](runtime::protocol::FrameMessage response) mutable {
                    if (runtime::protocol::is_cast_like(request.mode())) {
                        boost::asio::post(
                            self->session_context(),
                            [self]() {
                                self->finish_inflight_frame();
                            });
                        return;
                    }

                    std::string error_message;
                    auto encoded = RpcFrameCodec::encode_rpc_frame(
                        response,
                        self->options_.max_payload_bytes,
                        &error_message);
                    if (encoded.empty()) {
                        self->mark_closed();
                        self->finish_inflight_frame();
                        self->session_->async_close(TcpCloseReason::kEncodeFailed);
                        return;
                    }

                    boost::asio::post(
                        self->session_context(),
                        [self, encoded = std::move(encoded)]() mutable {
                            self->session_->async_write_frame(
                                std::move(encoded),
                                [self](bool write_ok) {
                                    if (!write_ok) {
                                        self->mark_closed();
                                        self->finish_inflight_frame();
                                        return;
                                    }
                                    self->finish_inflight_frame();
                                });
                        });
                });
        } catch (const std::exception& error) {
            handle_request_exception(request, error.what());
        } catch (...) {
            handle_request_exception(request, "rpc handler failed");
        }
    }

    boost::asio::ip::tcp::socket::executor_type session_context() {
        return session_->executor();
    }

    void handle_request_exception(
        const runtime::protocol::FrameMessage& request,
        const std::string& message) {
        if (runtime::protocol::is_cast_like(request.mode())) {
            auto self = shared_from_this();
            boost::asio::post(
                session_context(),
                [self]() {
                    self->finish_inflight_frame();
                });
            return;
        }
        auto response = runtime::protocol::make_error_frame(
            request,
            500,
            message.empty() ? "rpc handler failed" : message);
        std::string error_message;
        auto encoded = RpcFrameCodec::encode_rpc_frame(
            response,
            options_.max_payload_bytes,
            &error_message);
        if (encoded.empty()) {
            mark_closed();
            finish_inflight_frame();
            session_->async_close(TcpCloseReason::kEncodeFailed);
            return;
        }
        auto self = shared_from_this();
        boost::asio::post(
            session_context(),
            [self, encoded = std::move(encoded)]() mutable {
                self->session_->async_write_frame(
                    std::move(encoded),
                    [self](bool) {
                        self->finish_inflight_frame();
                    });
            });
    }

    void add_inflight_frame() {
        std::lock_guard<std::mutex> lock(mutex_);
        ++in_flight_frames_;
    }

    void finish_inflight_frame() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (in_flight_frames_ > 0) {
                --in_flight_frames_;
            }
        }
        maybe_start_read();
    }

    void mark_closed() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
    }

    std::shared_ptr<TcpSession> session_;
    RpcFrameHandler handler_;
    std::shared_ptr<runtime::scheduler::ShardedExecutor> handler_executor_;
    TransportOptions options_;
    std::mutex mutex_;
    std::uint32_t in_flight_frames_{};
    bool read_in_progress_{};
    bool closed_{};
};

}  // namespace

RpcFrameTransport::RpcFrameTransport(
    std::uint16_t port,
    RpcFrameHandler handler,
    std::string transport_name,
    TransportOptions options)
    : port_(port),
      handler_(std::move(handler)),
      transport_name_(std::move(transport_name)),
      options_(options) {}

int RpcFrameTransport::run() {
    if (!handler_) {
        return 1;
    }
    auto handler_executor =
        std::make_shared<runtime::scheduler::ShardedExecutor>(
            runtime::scheduler::ShardedExecutorOptions{
                options_.handler_shard_count,
                options_.max_handler_queue_depth});
    TcpListener listener(
        port_,
        [handler = handler_,
         handler_executor,
         options = options_](
            std::shared_ptr<TcpSession> session) mutable {
            std::make_shared<RpcFrameSession>(
                std::move(session),
                handler,
                handler_executor,
                options)
                ->start();
        },
        transport_name_,
        options_);
    return listener.run();
}

}  // namespace runtime::net

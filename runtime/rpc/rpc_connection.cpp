#include "runtime/rpc/rpc_connection.h"

#include <chrono>
#include <future>
#include <string>
#include <utility>
#include <vector>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

#include "runtime/net/rpc_frame_codec.h"
#include "runtime/protocol/frame.h"

namespace runtime::rpc {
namespace {

using TcpResolver = boost::asio::ip::tcp::resolver;
using TcpSocket = boost::asio::ip::tcp::socket;

int effective_timeout_millis(int override_timeout, int default_timeout) {
    return override_timeout > 0 ? override_timeout : default_timeout;
}

std::uint32_t max_rpc_frame_bytes(
    const runtime::net::TransportOptions& options) {
    return options.max_payload_bytes +
           runtime::net::RpcFrameCodec::kRpcFrameOverheadBytes;
}

RpcError encode_rpc_request(
    const runtime::protocol::FrameMessage& request,
    std::uint32_t max_payload_bytes,
    std::vector<std::uint8_t>* encoded) {
    if (encoded == nullptr) {
        return make_rpc_error(
            RpcErrorCode::kInvalidArgument,
            "encoded rpc frame output is required");
    }

    std::string error_message;
    *encoded = runtime::net::RpcFrameCodec::encode_rpc_frame(
        request,
        max_payload_bytes,
        &error_message);
    if (encoded->empty() && !error_message.empty()) {
        return make_rpc_error(RpcErrorCode::kEncodeFailed, error_message);
    }
    return {};
}

RpcError connect_error_from_asio(
    const boost::system::error_code& error,
    const std::string& fallback_message) {
    if (!error) {
        return {};
    }
    return make_rpc_error(
        RpcErrorCode::kConnectFailed,
        error.message().empty() ? fallback_message : error.message());
}

}  // namespace

RpcConnection::RpcConnection(
    runtime::net::TransportEndpoint endpoint,
    runtime::net::TransportOptions transport_options,
    RpcConnectionOptions rpc_connection_options,
    std::shared_ptr<runtime::scheduler::IOContextPool> io_context_pool)
    : endpoint_(std::move(endpoint)),
      transport_options_(transport_options),
      rpc_connection_options_(rpc_connection_options),
      io_context_pool_(std::move(io_context_pool)),
      pending_tracker_(rpc_connection_options.max_pending_requests) {}

RpcConnection::~RpcConnection() {
    close();
}

RpcResult RpcConnection::call(
    const runtime::protocol::FrameMessage& request,
    const RpcOptions& options) {
    if (request.request_id() == 0) {
        return RpcResult::failure(make_rpc_error(
            RpcErrorCode::kInvalidArgument,
            "rpc call request_id must be non-zero"));
    }

    const int timeout_millis = effective_timeout_millis(
        options.request_timeout_millis,
        rpc_connection_options_.request_timeout_millis);
    if (timeout_millis <= 0) {
        return RpcResult::failure(make_rpc_error(
            RpcErrorCode::kInvalidArgument,
            "rpc request timeout must be greater than zero"));
    }

    auto completed = std::make_shared<std::promise<RpcResult>>();
    auto future = completed->get_future();
    const auto error = call_async(
        request,
        options,
        [completed](RpcResult result) {
            completed->set_value(std::move(result));
        });
    if (!error.ok()) {
        return RpcResult::failure(error);
    }

    if (future.wait_for(std::chrono::milliseconds(timeout_millis)) !=
        std::future_status::ready) {
        remove_pending(request.request_id());
        return RpcResult::failure(make_rpc_error(
            RpcErrorCode::kTimeout, "rpc request timed out"));
    }
    return future.get();
}

RpcError RpcConnection::call_async(
    const runtime::protocol::FrameMessage& request,
    const RpcOptions& options,
    RpcResultHandler handler) {
    if (request.request_id() == 0) {
        return make_rpc_error(
            RpcErrorCode::kInvalidArgument,
            "rpc call request_id must be non-zero");
    }
    if (!handler) {
        return make_rpc_error(
            RpcErrorCode::kInvalidArgument,
            "rpc async handler is required");
    }

    const int timeout_millis = effective_timeout_millis(
        options.request_timeout_millis,
        rpc_connection_options_.request_timeout_millis);
    if (timeout_millis <= 0) {
        return make_rpc_error(
            RpcErrorCode::kInvalidArgument,
            "rpc request timeout must be greater than zero");
    }

    auto pending_call = std::make_shared<RpcPendingCall>();
    pending_call->handler = std::move(handler);
    pending_call->deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_millis);
    auto pending_error = pending_tracker_.add(request.request_id(), pending_call);
    if (!pending_error.ok()) {
        return pending_error;
    }

    std::vector<std::uint8_t> encoded;
    auto error = encode_rpc_request(
        request,
        transport_options_.max_payload_bytes,
        &encoded);
    if (error.ok()) {
        error = queue_or_send(std::move(encoded), options);
    }
    if (!error.ok()) {
        remove_pending(request.request_id());
        return error;
    }
    return {};
}

RpcResult RpcConnection::cast(
    const runtime::protocol::FrameMessage& request,
    const RpcOptions& options) {
    std::vector<std::uint8_t> encoded;
    auto error = encode_rpc_request(
        request,
        transport_options_.max_payload_bytes,
        &encoded);
    if (error.ok()) {
        error = queue_or_send(std::move(encoded), options);
    }
    if (!error.ok()) {
        return RpcResult::failure(std::move(error));
    }
    return RpcResult::accepted();
}

std::size_t RpcConnection::pending_count() const {
    return pending_tracker_.size();
}

void RpcConnection::close() {
    std::shared_ptr<runtime::net::TcpSession> session;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (closing_) {
            return;
        }
        closing_ = true;
        state_ = State::kClosed;
        ++connect_generation_;
        session = session_;
        session_.reset();
        outbound_queue_.clear();
    }

    if (session != nullptr) {
        session->async_close(runtime::net::TcpCloseReason::kHandlerStopped);
    }
    fail_all_pending(make_rpc_error(
        RpcErrorCode::kConnectionClosed, "rpc connection closed"));
    cancel_timeout_timer();
}

RpcError RpcConnection::queue_or_send(
    std::vector<std::uint8_t> encoded,
    const RpcOptions& options) {
    std::shared_ptr<runtime::net::TcpSession> session;
    bool should_connect = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (closing_ || state_ == State::kClosed) {
            return make_rpc_error(
                RpcErrorCode::kConnectionClosed,
                "rpc connection is closed");
        }
        if (state_ == State::kReady && session_ != nullptr) {
            session = session_;
        } else {
            if (outbound_queue_.size() >=
                transport_options_.max_write_queue_depth) {
                return make_rpc_error(
                    RpcErrorCode::kPendingLimitExceeded,
                    "rpc outbound queue is full");
            }
            outbound_queue_.push_back(std::move(encoded));
            if (state_ == State::kDisconnected) {
                state_ = State::kConnecting;
                should_connect = true;
            }
        }
    }

    if (session != nullptr) {
        send_encoded(std::move(encoded));
        return {};
    }
    if (should_connect) {
        auto error = start_connect(options);
        if (!error.ok()) {
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (state_ == State::kConnecting) {
                    state_ = State::kDisconnected;
                }
                outbound_queue_.clear();
            }
        }
        return error;
    }
    return {};
}

RpcError RpcConnection::start_connect(const RpcOptions& options) {
    std::lock_guard<std::mutex> connect_lock(connect_mutex_);
    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ == State::kReady && !closing_ && session_ != nullptr) {
            return {};
        }
        if (closing_) {
            return make_rpc_error(
                RpcErrorCode::kConnectionClosed,
                "rpc connection is closed");
        }
        generation = ++connect_generation_;
    }

    const int timeout_millis = effective_timeout_millis(
        options.connect_timeout_millis,
        rpc_connection_options_.connect_timeout_millis);
    if (timeout_millis <= 0) {
        return make_rpc_error(
            RpcErrorCode::kInvalidArgument,
            "rpc connect timeout must be greater than zero");
    }
    if (io_context_pool_ == nullptr) {
        return make_rpc_error(
            RpcErrorCode::kInvalidArgument,
            "rpc io context pool is not configured");
    }

    auto& io_context = io_context_pool_->next();
    auto self = shared_from_this();
    auto resolver = std::make_shared<TcpResolver>(io_context);
    auto socket = std::make_shared<TcpSocket>(io_context);
    auto connect_timer = std::make_shared<boost::asio::steady_timer>(io_context);

    connect_timer->expires_after(std::chrono::milliseconds(timeout_millis));
    connect_timer->async_wait(
        [self, socket, generation](const boost::system::error_code& error) {
            if (error) {
                return;
            }
            boost::system::error_code ignored;
            socket->close(ignored);
            self->finish_connect(
                generation,
                socket,
                make_rpc_error(
                    RpcErrorCode::kTimeout,
                    "rpc connect timed out"));
        });

    resolver->async_resolve(
        endpoint_.host,
        std::to_string(endpoint_.port),
        [self, resolver, socket, connect_timer, generation](
            const boost::system::error_code& resolve_error,
            const TcpResolver::results_type& endpoints) {
            if (resolve_error) {
                boost::system::error_code ignored;
                connect_timer->cancel(ignored);
                self->finish_connect(
                    generation,
                    socket,
                    connect_error_from_asio(
                        resolve_error,
                        "failed to resolve rpc endpoint"));
                return;
            }

            boost::asio::async_connect(
                *socket,
                endpoints,
                [self, socket, connect_timer, generation](
                    const boost::system::error_code& connect_error,
                    const TcpResolver::endpoint_type&) {
                    boost::system::error_code ignored;
                    connect_timer->cancel(ignored);
                    self->finish_connect(
                        generation,
                        socket,
                        connect_error_from_asio(
                            connect_error,
                            "failed to connect rpc endpoint"));
                });
        });
    return {};
}

void RpcConnection::finish_connect(
    std::uint64_t generation,
    std::shared_ptr<TcpSocket> socket,
    RpcError error) {
    if (!error.ok()) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (generation != connect_generation_ ||
                state_ != State::kConnecting) {
                return;
            }
            state_ = State::kDisconnected;
            outbound_queue_.clear();
        }
        fail_all_pending(std::move(error));
        return;
    }

    runtime::net::TcpSessionOptions session_options;
    session_options.max_write_queue_depth =
        transport_options_.max_write_queue_depth;
    auto self = shared_from_this();
    session_options.on_close =
        [self](runtime::net::TcpCloseReason) {
            self->disconnect(make_rpc_error(
                RpcErrorCode::kConnectionClosed,
                "rpc connection closed"));
        };

    auto session = std::make_shared<runtime::net::TcpSession>(
        std::move(*socket),
        std::move(session_options));
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (closing_ ||
            generation != connect_generation_ ||
            state_ != State::kConnecting) {
            return;
        }
        session_ = session;
        state_ = State::kReady;
    }

    timeout_timer_ = std::make_unique<boost::asio::steady_timer>(
        session->executor());
    start_read_loop();
    start_timeout_timer();
    flush_outbound_queue();
}

void RpcConnection::flush_outbound_queue() {
    std::deque<std::vector<std::uint8_t>> queue;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        queue.swap(outbound_queue_);
    }
    while (!queue.empty()) {
        send_encoded(std::move(queue.front()));
        queue.pop_front();
    }
}

void RpcConnection::send_encoded(std::vector<std::uint8_t> encoded) {
    std::shared_ptr<runtime::net::TcpSession> session;
    bool connection_not_ready = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != State::kReady || closing_ || session_ == nullptr) {
            connection_not_ready = true;
        } else {
            session = session_;
        }
    }
    if (connection_not_ready) {
        disconnect(make_rpc_error(
            RpcErrorCode::kConnectionClosed,
            "rpc connection is not ready"));
        return;
    }
    if (session == nullptr) {
        disconnect(make_rpc_error(
            RpcErrorCode::kConnectionClosed,
            "rpc session is not available"));
        return;
    }

    boost::asio::post(
        session->executor(),
        [self = shared_from_this(),
         session,
         encoded = std::move(encoded)]() mutable {
            session->async_write_frame(
                std::move(encoded),
                [self](bool ok) {
                    if (!ok) {
                        self->disconnect(make_rpc_error(
                            RpcErrorCode::kWriteFailed,
                            "failed to write rpc frame"));
                    }
                });
        });
}

void RpcConnection::start_read_loop() {
    std::shared_ptr<runtime::net::TcpSession> session;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        session = session_;
    }
    if (session == nullptr) {
        return;
    }

    session->async_read_frame(
        max_rpc_frame_bytes(transport_options_),
        [self = shared_from_this(),
         session](bool ok, std::vector<std::uint8_t> data) mutable {
            if (!ok) {
                self->disconnect(make_rpc_error(
                    RpcErrorCode::kReadFailed,
                    "failed to read rpc frame"));
                return;
            }

            runtime::protocol::FrameMessage response;
            std::string error_message;
            if (!runtime::net::RpcFrameCodec::decode_rpc_frame(
                    data,
                    self->transport_options_.max_payload_bytes,
                    &response,
                    &error_message)) {
                self->disconnect(make_rpc_error(
                    RpcErrorCode::kDecodeFailed,
                    error_message));
                return;
            }

            if (response.message_id() ==
                runtime::protocol::kErrorResponseMessageId) {
                self->pending_tracker_.complete(
                    response.request_id(),
                    RpcResult::remote_error(std::move(response)));
            } else {
                self->pending_tracker_.complete(
                    response.request_id(),
                    RpcResult::success(std::move(response)));
            }
            self->start_read_loop();
        });
}

void RpcConnection::start_timeout_timer() {
    if (timeout_timer_ == nullptr) {
        return;
    }
    timeout_timer_->expires_after(std::chrono::milliseconds(10));
    timeout_timer_->async_wait(
        [self = shared_from_this()](
            const boost::system::error_code& error) {
            if (error) {
                return;
            }
            self->pending_tracker_.expire(
                std::chrono::steady_clock::now(),
                make_rpc_error(
                    RpcErrorCode::kTimeout,
                    "rpc request timed out"));
            {
                std::lock_guard<std::mutex> lock(self->state_mutex_);
                if (self->state_ != State::kReady || self->closing_) {
                    return;
                }
            }
            self->start_timeout_timer();
        });
}

void RpcConnection::disconnect(RpcError error) {
    std::shared_ptr<runtime::net::TcpSession> session;
    bool already_disconnected = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != State::kReady && session_ == nullptr) {
            already_disconnected = true;
        } else {
            if (state_ != State::kClosed) {
                state_ = State::kDisconnected;
            }
            ++connect_generation_;
            session = session_;
            session_.reset();
            outbound_queue_.clear();
        }
    }

    if (already_disconnected) {
        fail_all_pending(std::move(error));
        return;
    }

    if (timeout_timer_ != nullptr) {
        cancel_timeout_timer();
    }
    if (session != nullptr) {
        session->async_close(runtime::net::TcpCloseReason::kHandlerStopped);
    }
    fail_all_pending(std::move(error));
}

void RpcConnection::fail_all_pending(RpcError error) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (state_ != State::kClosed) {
            state_ = State::kDisconnected;
        }
    }
    pending_tracker_.fail_all(error);
}

void RpcConnection::remove_pending(std::uint64_t request_id) {
    pending_tracker_.remove(request_id);
}

void RpcConnection::cancel_timeout_timer() {
    if (timeout_timer_ != nullptr) {
        boost::system::error_code ignored;
        timeout_timer_->cancel(ignored);
    }
}

}  // namespace runtime::rpc

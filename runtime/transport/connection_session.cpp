#include "runtime/transport/connection_session.h"

#include <chrono>
#include <cstring>
#include <exception>
#include <utility>

#include "runtime/observability/logging.h"
#include "runtime/protocol/envelope_utils.h"

namespace runtime::transport {

ConnectionSession::ConnectionSession(
    TcpSocket socket,
    EnvelopeHandler handler,
    std::string service_name,
    TransportOptions options,
    std::shared_ptr<runtime::execution::ShardedExecutor> handler_executor,
    std::shared_ptr<runtime::observability::MetricsRegistry> metrics)
    : socket_(std::move(socket)),
      handler_(std::move(handler)),
      service_name_(std::move(service_name)),
      options_(options),
      handler_executor_(std::move(handler_executor)),
      metrics_(std::move(metrics)) {}

void ConnectionSession::start() {
    if (metrics_ != nullptr) {
        metrics_->record_connection_open();
    }
    read_header();
}

void ConnectionSession::read_header() {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(header_),
        [self](const boost::system::error_code& error, std::size_t bytes_read) {
            if (error) {
                self->fail("tcp_read_header_failed", error.message());
                return;
            }
            if (self->metrics_ != nullptr) {
                self->metrics_->record_bytes_read(bytes_read);
            }

            std::uint32_t payload_size = 0;
            std::string error_message;
            if (!EnvelopeCodec::decode_payload_size(
                    self->header_,
                    self->options_.max_payload_bytes,
                    &payload_size,
                    &error_message)) {
                self->fail("tcp_invalid_envelope_header", error_message);
                return;
            }
            self->read_payload(payload_size);
        });
}

void ConnectionSession::read_payload(std::uint32_t payload_size) {
    payload_.assign(payload_size, '\0');
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(payload_),
        [self](const boost::system::error_code& error, std::size_t bytes_read) {
            if (error) {
                self->fail("tcp_read_payload_failed", error.message());
                return;
            }
            if (self->metrics_ != nullptr) {
                self->metrics_->record_bytes_read(bytes_read);
            }

            mmo::common::Envelope request;
            std::string error_message;
            if (!EnvelopeCodec::parse_payload(
                    self->payload_, &request, &error_message)) {
                self->fail("tcp_invalid_envelope_payload", error_message);
                return;
            }
            self->read_header();
            self->dispatch_request(std::move(request));
        });
}

void ConnectionSession::dispatch_request(mmo::common::Envelope request) {
    if (metrics_ != nullptr) {
        metrics_->record_request();
    }

    auto log_context =
        runtime::observability::context_from_envelope(service_name_, request);
    runtime::observability::log_info(log_context, "request_received");

    const auto started = std::chrono::steady_clock::now();
    auto self = shared_from_this();
    if (handler_executor_ != nullptr) {
        auto request_ptr =
            std::make_shared<mmo::common::Envelope>(std::move(request));
        const auto post_result = handler_executor_->post(
            shard_key(*request_ptr),
            [self, request_ptr, started]() {
                self->handle_request(*request_ptr, started);
            });
        if (metrics_ != nullptr) {
            metrics_->set_executor_queue_depth(handler_executor_->queued_task_count());
        }
        if (!post_result.accepted()) {
            if (metrics_ != nullptr) {
                metrics_->record_error();
                metrics_->record_executor_post_failed();
                metrics_->record_handler_rejected();
            }
            std::string error_message = "handler executor is stopped";
            if (post_result.status ==
                runtime::execution::PostStatus::kQueueFull) {
                error_message = "handler queue is full";
                if (metrics_ != nullptr) {
                    metrics_->record_executor_queue_overflow();
                }
            }
            write_response(runtime::protocol::make_error_envelope(
                *request_ptr, 503, error_message));
        }
        return;
    }

    handle_request(std::move(request), started);
}

void ConnectionSession::handle_request(
    mmo::common::Envelope request,
    TimePoint started) {
    mmo::common::Envelope response;
    auto log_context =
        runtime::observability::context_from_envelope(service_name_, request);
    try {
        response = handler_(request);
    } catch (const std::exception& error) {
        response = runtime::protocol::make_error_envelope(
            request, 500, std::string("handler exception: ") + error.what());
    } catch (...) {
        response = runtime::protocol::make_error_envelope(
            request, 500, "handler unknown exception");
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);
    log_context.latency_ms = elapsed.count();
    if (metrics_ != nullptr) {
        metrics_->record_handler_latency_ms(
            static_cast<std::uint64_t>(elapsed.count()));
        if (handler_executor_ != nullptr) {
            metrics_->set_executor_queue_depth(handler_executor_->queued_task_count());
        }
    }

    if (response.message_type() == runtime::protocol::kErrorResponse) {
        log_context.error_code = 1;
        if (metrics_ != nullptr) {
            metrics_->record_error();
        }
        runtime::observability::log_error(log_context, "request_failed");
    } else {
        runtime::observability::log_info(log_context, "request_handled");
    }

    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [self, response = std::move(response)]() {
        self->write_response(response);
    });
}

void ConnectionSession::write_response(const mmo::common::Envelope& response) {
    std::string payload;
    std::string error_message;
    if (!EnvelopeCodec::serialize_payload(
            response, options_.max_payload_bytes, &payload, &error_message)) {
        fail("tcp_response_encode_failed", error_message);
        return;
    }

    const auto header = EnvelopeCodec::encode_payload_size(
        static_cast<std::uint32_t>(payload.size()));
    std::string response_buffer;
    response_buffer.resize(header.size() + payload.size());
    std::memcpy(response_buffer.data(), header.data(), header.size());
    if (!payload.empty()) {
        std::memcpy(
            response_buffer.data() + header.size(),
            payload.data(),
            payload.size());
    }
    enqueue_response(std::move(response_buffer));
}

void ConnectionSession::enqueue_response(std::string response_buffer) {
    if (closed_) {
        return;
    }

    const bool should_start_write = !writing_;
    write_queue_.push_back(std::move(response_buffer));
    if (should_start_write) {
        write_next_response();
    }
}

void ConnectionSession::write_next_response() {
    if (closed_ || write_queue_.empty()) {
        writing_ = false;
        return;
    }

    writing_ = true;

    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_queue_.front()),
        [self](const boost::system::error_code& error, std::size_t bytes_written) {
            if (error) {
                self->fail("tcp_response_write_failed", error.message());
                return;
            }
            if (self->metrics_ != nullptr) {
                self->metrics_->record_bytes_written(bytes_written);
            }
            self->write_queue_.pop_front();
            self->write_next_response();
        });
}

void ConnectionSession::fail(const std::string& event, const std::string& detail) {
    if (metrics_ != nullptr) {
        metrics_->record_error();
    }
    runtime::observability::log_warn(
        runtime::observability::LogContext{service_name_},
        event + " detail=" + detail);
    close();
}

void ConnectionSession::close() {
    if (closed_) {
        return;
    }
    closed_ = true;

    boost::system::error_code ignored;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    socket_.close(ignored);
    if (metrics_ != nullptr) {
        metrics_->record_connection_close();
    }
}

std::uint64_t ConnectionSession::shard_key(const mmo::common::Envelope& request) {
    if (request.player_id() > 0) {
        return static_cast<std::uint64_t>(request.player_id());
    }
    return request.request_id();
}

}  // namespace runtime::transport

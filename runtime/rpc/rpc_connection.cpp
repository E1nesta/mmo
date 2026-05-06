#include "runtime/rpc/rpc_connection.h"

#include <array>
#include <chrono>
#include <istream>
#include <ostream>
#include <thread>
#include <utility>
#include <vector>

#include "runtime/net/frame_codec.h"
#include "runtime/protocol/frame.h"

namespace runtime::rpc {
namespace {

bool read_exact(std::istream& input, void* buffer, std::size_t size) {
    input.read(static_cast<char*>(buffer), static_cast<std::streamsize>(size));
    return input.good() || input.gcount() == static_cast<std::streamsize>(size);
}

bool write_exact(std::ostream& output, const void* buffer, std::size_t size) {
    output.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(size));
    output.flush();
    return static_cast<bool>(output);
}

std::uint32_t decode_u32(const std::array<char, 4>& header) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(header[0])) << 24U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(header[1])) << 16U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(header[2])) << 8U) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(header[3]));
}

bool read_frame(
    std::istream& input,
    std::uint32_t max_payload_bytes,
    runtime::protocol::FrameMessage* frame,
    RpcError* error) {
    std::array<char, runtime::net::FrameCodec::kFrameLengthBytes> header{};
    if (!read_exact(input, header.data(), header.size())) {
        if (error != nullptr) {
            *error = make_rpc_error(
                RpcErrorCode::kReadFailed, "failed to read rpc frame header");
        }
        return false;
    }

    const auto body_size = decode_u32(header);
    if (body_size > max_payload_bytes) {
        if (error != nullptr) {
            *error = make_rpc_error(
                RpcErrorCode::kDecodeFailed, "rpc frame exceeds max size");
        }
        return false;
    }

    std::vector<std::uint8_t> data;
    data.reserve(header.size() + body_size);
    data.insert(data.end(), header.begin(), header.end());
    const auto old_size = data.size();
    data.resize(old_size + body_size);
    if (body_size > 0 &&
        !read_exact(input, data.data() + old_size, body_size)) {
        if (error != nullptr) {
            *error = make_rpc_error(
                RpcErrorCode::kReadFailed, "failed to read rpc frame body");
        }
        return false;
    }

    std::string error_message;
    if (!runtime::net::FrameCodec::decode_rpc_frame(
            data, max_payload_bytes, frame, &error_message)) {
        if (error != nullptr) {
            *error = make_rpc_error(RpcErrorCode::kDecodeFailed, error_message);
        }
        return false;
    }
    return true;
}

bool write_frame(
    std::ostream& output,
    const runtime::protocol::FrameMessage& frame,
    std::uint32_t max_payload_bytes,
    RpcError* error) {
    std::string error_message;
    const auto data = runtime::net::FrameCodec::encode_rpc_frame(
        frame, max_payload_bytes, &error_message);
    if (data.empty() && !error_message.empty()) {
        if (error != nullptr) {
            *error = make_rpc_error(RpcErrorCode::kEncodeFailed, error_message);
        }
        return false;
    }
    if (!write_exact(output, data.data(), data.size())) {
        if (error != nullptr) {
            *error = make_rpc_error(
                RpcErrorCode::kWriteFailed, "failed to write rpc frame");
        }
        return false;
    }
    return true;
}

}  // namespace

RpcConnection::RpcConnection(
    runtime::net::TransportEndpoint endpoint,
    runtime::net::TransportOptions transport_options,
    RpcConnectionOptions rpc_connection_options)
    : endpoint_(std::move(endpoint)),
      transport_options_(transport_options),
      rpc_connection_options_(rpc_connection_options),
      pending_tracker_(rpc_connection_options.max_pending_requests) {}

RpcConnection::~RpcConnection() {
    close();
}

RpcResult RpcConnection::call(
    const runtime::protocol::FrameMessage& request,
    const RpcCallOptions& options) {
    RpcError error;
    if (!ensure_connected(options, &error)) {
        return RpcResult::failure(std::move(error));
    }

    auto pending_call = std::make_shared<RpcPendingCall>();
    auto future = pending_call->promise.get_future();
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (closing_) {
            return RpcResult::failure(make_rpc_error(
                RpcErrorCode::kConnectionClosed, "rpc connection is closed"));
        }
    }
    auto pending_error = pending_tracker_.add(request.request_id(), pending_call);
    if (!pending_error.ok()) {
        return RpcResult::failure(std::move(pending_error));
    }

    if (!write_request(request, &error)) {
        remove_pending(request.request_id());
        disconnect(error);
        return RpcResult::failure(std::move(error));
    }

    const int timeout_millis =
        options.request_timeout_millis > 0
            ? options.request_timeout_millis
            : rpc_connection_options_.request_timeout_millis;
    if (future.wait_for(std::chrono::milliseconds(timeout_millis)) !=
        std::future_status::ready) {
        remove_pending(request.request_id());
        return RpcResult::failure(make_rpc_error(
            RpcErrorCode::kTimeout, "rpc request timed out"));
    }
    return future.get();
}

RpcResult RpcConnection::cast(
    const runtime::protocol::FrameMessage& request,
    const RpcCallOptions& options) {
    RpcError error;
    if (!ensure_connected(options, &error)) {
        return RpcResult::failure(std::move(error));
    }
    if (!write_request(request, &error)) {
        disconnect(error);
        return RpcResult::failure(std::move(error));
    }
    return RpcResult::accepted();
}

std::size_t RpcConnection::pending_count() const {
    return pending_tracker_.size();
}

void RpcConnection::close() {
    std::lock_guard<std::mutex> stream_lock(stream_mutex_);
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (closing_) {
            return;
        }
        closing_ = true;
    }

    stream_.close();
    fail_all_pending(make_rpc_error(
        RpcErrorCode::kConnectionClosed, "rpc connection closed"));
    if (reader_thread_.joinable() &&
        reader_thread_.get_id() != std::this_thread::get_id()) {
        reader_thread_.join();
    }
}

bool RpcConnection::ensure_connected(
    const RpcCallOptions& options,
    RpcError* error) {
    std::lock_guard<std::mutex> stream_lock(stream_mutex_);
    join_stopped_reader();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (connected_ && !closing_) {
            return true;
        }
        if (closing_) {
            if (error != nullptr) {
                *error = make_rpc_error(
                    RpcErrorCode::kConnectionClosed, "rpc connection is closed");
            }
            return false;
        }
    }

    const int timeout_millis =
        options.connect_timeout_millis > 0
            ? options.connect_timeout_millis
            : rpc_connection_options_.connect_timeout_millis;
    stream_.close();
    stream_.clear();
    stream_.expires_after(std::chrono::milliseconds(timeout_millis));
    stream_.connect(endpoint_.host, std::to_string(endpoint_.port));
    if (!stream_) {
        if (error != nullptr) {
            *error = make_rpc_error(
                RpcErrorCode::kConnectFailed,
                "failed to connect rpc endpoint");
        }
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        connected_ = true;
        reader_running_ = true;
    }
    reader_thread_ = std::thread([this]() { read_loop(); });
    return true;
}

bool RpcConnection::write_request(
    const runtime::protocol::FrameMessage& request,
    RpcError* error) {
    std::lock_guard<std::mutex> stream_lock(stream_mutex_);
    std::lock_guard<std::mutex> lock(write_mutex_);
    return write_frame(
        stream_, request, transport_options_.max_payload_bytes, error);
}

void RpcConnection::disconnect(RpcError error) {
    std::lock_guard<std::mutex> stream_lock(stream_mutex_);
    stream_.close();
    fail_all_pending(std::move(error));
    if (reader_thread_.joinable() &&
        reader_thread_.get_id() != std::this_thread::get_id()) {
        reader_thread_.join();
    }
}

void RpcConnection::join_stopped_reader() {
    if (!reader_thread_.joinable() ||
        reader_thread_.get_id() == std::this_thread::get_id()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (reader_running_) {
            return;
        }
    }
    reader_thread_.join();
}

void RpcConnection::read_loop() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (closing_) {
                reader_running_ = false;
                return;
            }
        }

        runtime::protocol::FrameMessage response;
        RpcError error;
        if (!read_frame(
                stream_, transport_options_.max_payload_bytes, &response, &error)) {
            fail_all_pending(std::move(error));
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                reader_running_ = false;
            }
            return;
        }

        if (response.message_id() == runtime::protocol::kErrorResponseMessageId) {
            pending_tracker_.complete(
                response.request_id(),
                RpcResult::remote_error(std::move(response)));
        } else {
            pending_tracker_.complete(
                response.request_id(),
                RpcResult::success(std::move(response)));
        }
    }
}

void RpcConnection::fail_all_pending(RpcError error) {
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        connected_ = false;
    }
    pending_tracker_.fail_all(error);
}

void RpcConnection::remove_pending(std::uint64_t request_id) {
    pending_tracker_.remove(request_id);
}

}  // namespace runtime::rpc

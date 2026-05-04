#include "runtime/channel/channel.h"

#include <array>
#include <chrono>
#include <istream>
#include <ostream>
#include <thread>
#include <utility>

#include "runtime/protocol/envelope_utils.h"
#include "runtime/transport/envelope_codec.h"

namespace runtime::channel {
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

bool read_envelope(
    std::istream& input,
    std::uint32_t max_payload_bytes,
    mmo::common::Envelope* envelope,
    ChannelError* error) {
    std::array<
        char,
        runtime::transport::EnvelopeCodec::kHeaderBytes> header{};
    if (!read_exact(input, header.data(), header.size())) {
        if (error != nullptr) {
            *error = make_channel_error(
                ChannelErrorCode::kReadFailed, "failed to read channel header");
        }
        return false;
    }

    std::uint32_t payload_size = 0;
    std::string error_message;
    if (!runtime::transport::EnvelopeCodec::decode_payload_size(
            header, max_payload_bytes, &payload_size, &error_message)) {
        if (error != nullptr) {
            *error = make_channel_error(ChannelErrorCode::kDecodeFailed, error_message);
        }
        return false;
    }

    std::string payload(payload_size, '\0');
    if (payload_size > 0 &&
        !read_exact(input, payload.data(), static_cast<std::size_t>(payload_size))) {
        if (error != nullptr) {
            *error = make_channel_error(
                ChannelErrorCode::kReadFailed, "failed to read channel payload");
        }
        return false;
    }

    if (!runtime::transport::EnvelopeCodec::parse_payload(
            payload, envelope, &error_message)) {
        if (error != nullptr) {
            *error = make_channel_error(ChannelErrorCode::kDecodeFailed, error_message);
        }
        return false;
    }
    return true;
}

bool write_envelope(
    std::ostream& output,
    const mmo::common::Envelope& envelope,
    std::uint32_t max_payload_bytes,
    ChannelError* error) {
    std::string payload;
    std::string error_message;
    if (!runtime::transport::EnvelopeCodec::serialize_payload(
            envelope, max_payload_bytes, &payload, &error_message)) {
        if (error != nullptr) {
            *error = make_channel_error(ChannelErrorCode::kEncodeFailed, error_message);
        }
        return false;
    }

    const auto header =
        runtime::transport::EnvelopeCodec::encode_payload_size(
            static_cast<std::uint32_t>(payload.size()));
    if (!write_exact(output, header.data(), header.size()) ||
        !write_exact(output, payload.data(), payload.size())) {
        if (error != nullptr) {
            *error = make_channel_error(
                ChannelErrorCode::kWriteFailed,
                "failed to write channel request");
        }
        return false;
    }
    return true;
}

}  // namespace

Channel::Channel(
    runtime::transport::TransportEndpoint endpoint,
    runtime::transport::TransportOptions transport_options,
    ChannelOptions channel_options)
    : endpoint_(std::move(endpoint)),
      transport_options_(transport_options),
      channel_options_(channel_options) {}

Channel::~Channel() {
    close();
}

ChannelResult Channel::call(
    const mmo::common::Envelope& request,
    const ChannelCallOptions& options) {
    ChannelError error;
    if (!ensure_connected(options, &error)) {
        return ChannelResult::failure(std::move(error));
    }

    auto pending_call = std::make_shared<PendingCall>();
    auto future = pending_call->promise.get_future();
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (closing_) {
            return ChannelResult::failure(make_channel_error(
                ChannelErrorCode::kChannelClosed, "channel is closed"));
        }
        if (pending_.size() >= channel_options_.max_pending_requests) {
            return ChannelResult::failure(make_channel_error(
                ChannelErrorCode::kPendingLimitExceeded,
                "channel pending limit exceeded"));
        }
        if (pending_.find(request.request_id()) != pending_.end()) {
            return ChannelResult::failure(make_channel_error(
                ChannelErrorCode::kDuplicateRequestId,
                "channel request_id is already pending"));
        }
        pending_[request.request_id()] = pending_call;
    }

    if (!write_request(request, &error)) {
        remove_pending(request.request_id());
        disconnect(error);
        return ChannelResult::failure(std::move(error));
    }

    const int timeout_millis =
        options.request_timeout_millis > 0
            ? options.request_timeout_millis
            : channel_options_.request_timeout_millis;
    if (future.wait_for(std::chrono::milliseconds(timeout_millis)) !=
        std::future_status::ready) {
        remove_pending(request.request_id());
        return ChannelResult::failure(make_channel_error(
            ChannelErrorCode::kTimeout, "channel request timed out"));
    }
    return future.get();
}

std::size_t Channel::pending_count() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return pending_.size();
}

void Channel::close() {
    std::lock_guard<std::mutex> stream_lock(stream_mutex_);
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (closing_) {
            return;
        }
        closing_ = true;
    }

    stream_.close();
    fail_all_pending(make_channel_error(
        ChannelErrorCode::kChannelClosed, "channel closed"));
    if (reader_thread_.joinable() &&
        reader_thread_.get_id() != std::this_thread::get_id()) {
        reader_thread_.join();
    }
}

bool Channel::ensure_connected(
    const ChannelCallOptions& options,
    ChannelError* error) {
    std::lock_guard<std::mutex> stream_lock(stream_mutex_);
    join_stopped_reader();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (connected_ && !closing_) {
            return true;
        }
        if (closing_) {
            if (error != nullptr) {
                *error = make_channel_error(
                    ChannelErrorCode::kChannelClosed, "channel is closed");
            }
            return false;
        }
    }

    const int timeout_millis =
        options.connect_timeout_millis > 0
            ? options.connect_timeout_millis
            : channel_options_.connect_timeout_millis;
    stream_.close();
    stream_.clear();
    stream_.expires_after(std::chrono::milliseconds(timeout_millis));
    stream_.connect(endpoint_.host, std::to_string(endpoint_.port));
    if (!stream_) {
        if (error != nullptr) {
            *error = make_channel_error(
                ChannelErrorCode::kConnectFailed,
                "failed to connect channel endpoint");
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

bool Channel::write_request(
    const mmo::common::Envelope& request,
    ChannelError* error) {
    std::lock_guard<std::mutex> stream_lock(stream_mutex_);
    std::lock_guard<std::mutex> lock(write_mutex_);
    return write_envelope(
        stream_, request, transport_options_.max_payload_bytes, error);
}

void Channel::disconnect(ChannelError error) {
    std::lock_guard<std::mutex> stream_lock(stream_mutex_);
    stream_.close();
    fail_all_pending(std::move(error));
    if (reader_thread_.joinable() &&
        reader_thread_.get_id() != std::this_thread::get_id()) {
        reader_thread_.join();
    }
}

void Channel::join_stopped_reader() {
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

void Channel::read_loop() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (closing_) {
                reader_running_ = false;
                return;
            }
        }

        mmo::common::Envelope response;
        ChannelError error;
        if (!read_envelope(
                stream_, transport_options_.max_payload_bytes, &response, &error)) {
            fail_all_pending(std::move(error));
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                reader_running_ = false;
            }
            return;
        }

        std::shared_ptr<PendingCall> pending_call;
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            const auto it = pending_.find(response.request_id());
            if (it != pending_.end()) {
                pending_call = it->second;
                pending_.erase(it);
            }
        }
        if (pending_call == nullptr) {
            continue;
        }

        if (response.message_type() == runtime::protocol::kErrorResponse) {
            pending_call->promise.set_value(
                ChannelResult::remote_error(std::move(response)));
        } else {
            pending_call->promise.set_value(
                ChannelResult::success(std::move(response)));
        }
    }
}

void Channel::fail_all_pending(ChannelError error) {
    std::unordered_map<std::uint64_t, std::shared_ptr<PendingCall>> pending;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        connected_ = false;
        pending.swap(pending_);
    }
    for (auto& [request_id, pending_call] : pending) {
        (void)request_id;
        pending_call->promise.set_value(ChannelResult::failure(error));
    }
}

void Channel::remove_pending(std::uint64_t request_id) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    pending_.erase(request_id);
}

}  // namespace runtime::channel

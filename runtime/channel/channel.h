#pragma once

#include <cstddef>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <boost/asio.hpp>

#include "runtime/channel/channel_call_options.h"
#include "runtime/channel/channel_result.h"
#include "runtime/transport/envelope_transport.h"

namespace runtime::channel {

struct ChannelOptions {
    int connect_timeout_millis{};
    int request_timeout_millis{};
    std::size_t max_pending_requests{};
};

class Channel {
public:
    Channel(
        runtime::transport::TransportEndpoint endpoint,
        runtime::transport::TransportOptions transport_options,
        ChannelOptions channel_options);
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    ~Channel();

    ChannelResult call(
        const mmo::common::Envelope& request,
        const ChannelCallOptions& options);
    std::size_t pending_count() const;
    void close();

private:
    struct PendingCall {
        std::promise<ChannelResult> promise;
    };

    bool ensure_connected(const ChannelCallOptions& options, ChannelError* error);
    bool write_request(const mmo::common::Envelope& request, ChannelError* error);
    void disconnect(ChannelError error);
    void join_stopped_reader();
    void read_loop();
    void fail_all_pending(ChannelError error);
    void remove_pending(std::uint64_t request_id);

    runtime::transport::TransportEndpoint endpoint_;
    runtime::transport::TransportOptions transport_options_;
    ChannelOptions channel_options_;
    boost::asio::ip::tcp::iostream stream_;
    std::thread reader_thread_;
    mutable std::mutex state_mutex_;
    std::mutex stream_mutex_;
    std::mutex write_mutex_;
    std::unordered_map<std::uint64_t, std::shared_ptr<PendingCall>> pending_;
    bool connected_{};
    bool closing_{};
    bool reader_running_{};
};

}  // namespace runtime::channel

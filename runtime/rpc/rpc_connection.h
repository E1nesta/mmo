#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>

#include <boost/asio.hpp>

#include "runtime/net/frame_transport.h"
#include "runtime/protocol/frame.h"
#include "runtime/rpc/rpc_call_options.h"
#include "runtime/rpc/rpc_pending_tracker.h"
#include "runtime/rpc/rpc_result.h"

namespace runtime::rpc {

struct RpcConnectionOptions {
    int connect_timeout_millis{};
    int request_timeout_millis{};
    std::size_t max_pending_requests{};
};

class RpcConnection {
public:
    RpcConnection(
        runtime::net::TransportEndpoint endpoint,
        runtime::net::TransportOptions transport_options,
        RpcConnectionOptions rpc_connection_options);
    RpcConnection(const RpcConnection&) = delete;
    RpcConnection& operator=(const RpcConnection&) = delete;
    ~RpcConnection();

    RpcResult call(
        const runtime::protocol::FrameMessage& request,
        const RpcCallOptions& options);
    RpcResult cast(
        const runtime::protocol::FrameMessage& request,
        const RpcCallOptions& options);
    std::size_t pending_count() const;
    void close();

private:
    bool ensure_connected(const RpcCallOptions& options, RpcError* error);
    bool write_request(
        const runtime::protocol::FrameMessage& request,
        RpcError* error);
    void disconnect(RpcError error);
    void join_stopped_reader();
    void read_loop();
    void fail_all_pending(RpcError error);
    void remove_pending(std::uint64_t request_id);

    runtime::net::TransportEndpoint endpoint_;
    runtime::net::TransportOptions transport_options_;
    RpcConnectionOptions rpc_connection_options_;
    RpcPendingTracker pending_tracker_;
    boost::asio::ip::tcp::iostream stream_;
    std::thread reader_thread_;
    mutable std::mutex state_mutex_;
    std::mutex stream_mutex_;
    std::mutex write_mutex_;
    bool connected_{};
    bool closing_{};
    bool reader_running_{};
};

}  // namespace runtime::rpc

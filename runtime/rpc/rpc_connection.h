#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

#include <boost/asio.hpp>

#include "runtime/net/frame_transport.h"
#include "runtime/net/tcp_session.h"
#include "runtime/protocol/frame.h"
#include "runtime/rpc/rpc_options.h"
#include "runtime/rpc/rpc_pending_tracker.h"
#include "runtime/rpc/rpc_result.h"
#include "runtime/scheduler/io_context_pool.h"

namespace runtime::rpc {

struct RpcConnectionOptions {
    int connect_timeout_millis{};
    int request_timeout_millis{};
    std::size_t max_pending_requests{};
};

class RpcConnection : public std::enable_shared_from_this<RpcConnection> {
public:
    RpcConnection(
        runtime::net::TransportEndpoint endpoint,
        runtime::net::TransportOptions transport_options,
        RpcConnectionOptions rpc_connection_options,
        std::shared_ptr<runtime::scheduler::IOContextPool> io_context_pool);
    RpcConnection(const RpcConnection&) = delete;
    RpcConnection& operator=(const RpcConnection&) = delete;
    ~RpcConnection();

    RpcResult call(
        const runtime::protocol::FrameMessage& request,
        const RpcOptions& options);
    RpcError call_async(
        const runtime::protocol::FrameMessage& request,
        const RpcOptions& options,
        RpcResultHandler handler);
    RpcResult cast(
        const runtime::protocol::FrameMessage& request,
        const RpcOptions& options);
    std::size_t pending_count() const;
    void close();

private:
    enum class State {
        kDisconnected,
        kConnecting,
        kReady,
        kClosed,
    };

    RpcError queue_or_send(
        std::vector<std::uint8_t> encoded,
        const RpcOptions& options);
    RpcError start_connect(const RpcOptions& options);
    void finish_connect(
        std::uint64_t generation,
        std::shared_ptr<boost::asio::ip::tcp::socket> socket,
        RpcError error);
    void flush_outbound_queue();
    void send_encoded(std::vector<std::uint8_t> encoded);
    void start_read_loop();
    void start_timeout_timer();
    void disconnect(RpcError error);
    void fail_all_pending(RpcError error);
    void remove_pending(std::uint64_t request_id);
    void cancel_timeout_timer();

    runtime::net::TransportEndpoint endpoint_;
    runtime::net::TransportOptions transport_options_;
    RpcConnectionOptions rpc_connection_options_;
    std::shared_ptr<runtime::scheduler::IOContextPool> io_context_pool_;
    RpcPendingTracker pending_tracker_;
    std::unique_ptr<boost::asio::steady_timer> timeout_timer_;
    std::shared_ptr<runtime::net::TcpSession> session_;
    std::deque<std::vector<std::uint8_t>> outbound_queue_;
    mutable std::mutex state_mutex_;
    std::mutex connect_mutex_;
    std::uint64_t connect_generation_{};
    State state_{State::kDisconnected};
    bool closing_{};
};

}  // namespace runtime::rpc

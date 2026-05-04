#pragma once

#include <array>
#include <chrono>
#include <deque>
#include <memory>
#include <string>

#include <boost/asio.hpp>

#include "runtime/execution/sharded_executor.h"
#include "runtime/observability/metrics.h"
#include "runtime/transport/envelope_codec.h"
#include "runtime/transport/envelope_transport.h"

namespace runtime::transport {

class ConnectionSession : public std::enable_shared_from_this<ConnectionSession> {
public:
    using TcpSocket = boost::asio::ip::tcp::socket;

    ConnectionSession(
        TcpSocket socket,
        EnvelopeHandler handler,
        std::string service_name,
        TransportOptions options,
        std::shared_ptr<runtime::execution::ShardedExecutor> handler_executor,
        std::shared_ptr<runtime::observability::MetricsRegistry> metrics);

    void start();

private:
    using TimePoint = std::chrono::steady_clock::time_point;

    void read_header();
    void read_payload(std::uint32_t payload_size);
    void dispatch_request(mmo::common::Envelope request);
    void handle_request(mmo::common::Envelope request, TimePoint started);
    void write_response(const mmo::common::Envelope& response);
    void enqueue_response(std::string response_buffer);
    void write_next_response();
    void fail(const std::string& event, const std::string& detail);
    void close();

    static std::uint64_t shard_key(const mmo::common::Envelope& request);

    TcpSocket socket_;
    EnvelopeHandler handler_;
    std::string service_name_;
    TransportOptions options_;
    std::shared_ptr<runtime::execution::ShardedExecutor> handler_executor_;
    std::shared_ptr<runtime::observability::MetricsRegistry> metrics_;
    std::array<char, EnvelopeCodec::kHeaderBytes> header_{};
    std::string payload_;
    std::deque<std::string> write_queue_;
    bool writing_{};
    bool closed_{};
};

}  // namespace runtime::transport

#include "runtime/transport/tcp_envelope_server.h"

#include <boost/asio.hpp>

#include <algorithm>
#include <csignal>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "runtime/execution/io_context_pool.h"
#include "runtime/observability/logging.h"
#include "runtime/transport/connection_session.h"

namespace runtime::transport {
namespace {

using boost::asio::ip::tcp;

std::size_t positive_count(int value) {
    return static_cast<std::size_t>(std::max(1, value));
}

}  // namespace

TcpEnvelopeServer::TcpEnvelopeServer(
    std::uint16_t port,
    EnvelopeHandler handler,
    std::string service_name,
    TransportOptions options,
    std::shared_ptr<runtime::execution::ShardedExecutor> handler_executor,
    std::shared_ptr<runtime::observability::MetricsRegistry> metrics)
    : port_(port),
      handler_(std::move(handler)),
      service_name_(std::move(service_name)),
      options_(options),
      handler_executor_(std::move(handler_executor)),
      metrics_(std::move(metrics)) {}

int TcpEnvelopeServer::run() {
    try {
        runtime::execution::IOContextPool io_context_pool(
            positive_count(options_.io_thread_count));
        if (handler_executor_ == nullptr) {
            handler_executor_ = std::make_shared<runtime::execution::ShardedExecutor>(
                runtime::execution::ShardedExecutorOptions{
                    positive_count(options_.handler_shard_count),
                    options_.max_handler_queue_depth_per_shard});
        }
        if (metrics_ == nullptr) {
            metrics_ =
                std::make_shared<runtime::observability::MetricsRegistry>();
        }

        auto& accept_context = io_context_pool.next();
        tcp::acceptor acceptor(accept_context);
        const tcp::endpoint endpoint(tcp::v4(), port_);
        acceptor.open(endpoint.protocol());
        acceptor.set_option(tcp::acceptor::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen(options_.listen_backlog);

        boost::asio::signal_set signals(accept_context, SIGINT, SIGTERM);
        signals.async_wait(
            [&](const boost::system::error_code&, int) {
                boost::system::error_code ignored;
                acceptor.close(ignored);
                io_context_pool.stop();
                handler_executor_->stop();
            });

        std::function<void()> accept_next;
        accept_next = [&]() {
            auto socket = std::make_shared<tcp::socket>(io_context_pool.next());
            acceptor.async_accept(
                *socket,
                [&, socket](const boost::system::error_code& error) {
                    if (!error) {
                        std::make_shared<ConnectionSession>(
                            std::move(*socket),
                            handler_,
                            service_name_,
                            options_,
                            handler_executor_,
                            metrics_)
                            ->start();
                    } else if (acceptor.is_open()) {
                        runtime::observability::log_error(
                            runtime::observability::LogContext{service_name_},
                            "tcp_accept_failed error=" + error.message());
                    }

                    if (acceptor.is_open()) {
                        accept_next();
                    }
                });
        };
        accept_next();

        runtime::observability::log_info(
            runtime::observability::LogContext{service_name_},
                "tcp_server_listening port=" + std::to_string(port_) +
                " io_threads=" + std::to_string(io_context_pool.size()) +
                " handler_shards=" +
                std::to_string(handler_executor_->shard_count()) +
                " max_handler_queue_depth_per_shard=" +
                std::to_string(options_.max_handler_queue_depth_per_shard));

        io_context_pool.run();
        handler_executor_->stop();
        return 0;
    } catch (const std::exception& error) {
        runtime::observability::log_error(
            runtime::observability::LogContext{service_name_},
            std::string("tcp_server_fatal error=") + error.what());
        return 1;
    }
}

}  // namespace runtime::transport

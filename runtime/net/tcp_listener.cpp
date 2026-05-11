#include "runtime/net/tcp_listener.h"

#include <atomic>
#include <memory>
#include <utility>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/system/error_code.hpp>

namespace runtime::net {

TcpListener::TcpListener(
    std::uint16_t port,
    SessionHandler handler,
    std::string listener_name,
    TransportOptions options)
    : port_(port),
      handler_(std::move(handler)),
      listener_name_(std::move(listener_name)),
      options_(options),
      counters_(std::make_shared<TransportCounters>()),
      io_context_pool_(options.io_thread_count > 0
          ? options.io_thread_count
          : kDefaultIoThreadCount) {}

int TcpListener::run() {
    (void)listener_name_;
    if (!handler_) {
        return 1;
    }

    boost::system::error_code error;
    const boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::tcp::v4(),
        port_);
    auto acceptor = std::make_shared<boost::asio::ip::tcp::acceptor>(
        io_context_pool_.next());
    acceptor->open(endpoint.protocol(), error);
    if (error) {
        return 1;
    }
    acceptor->set_option(boost::asio::socket_base::reuse_address(true), error);
    if (error) {
        return 1;
    }
    acceptor->bind(endpoint, error);
    if (error) {
        return 1;
    }
    acceptor->listen(options_.listen_backlog, error);
    if (error) {
        return 1;
    }

    auto do_accept = std::make_shared<std::function<void()>>();
    *do_accept = [this, acceptor, do_accept]() {
        auto& session_context = io_context_pool_.next();
        auto* raw_session_context = &session_context;
        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(
            session_context);
        acceptor->async_accept(
            *socket,
            [this, acceptor, do_accept, raw_session_context, socket](
                const boost::system::error_code& accept_error) {
                if (accept_error) {
                    counters_->errors.fetch_add(1, std::memory_order_relaxed);
                    (*do_accept)();
                    return;
                }

                const auto active =
                    counters_->active_connections.load(std::memory_order_relaxed);
                if (active >= options_.max_connections) {
                    counters_->rejected_connections.fetch_add(
                        1,
                        std::memory_order_relaxed);
                    boost::system::error_code ignored;
                    socket->shutdown(
                        boost::asio::ip::tcp::socket::shutdown_both,
                        ignored);
                    socket->close(ignored);
                    (*do_accept)();
                    return;
                }

                counters_->active_connections.fetch_add(
                    1,
                    std::memory_order_relaxed);
                counters_->accepted_connections.fetch_add(
                    1,
                    std::memory_order_relaxed);

                auto counters = counters_;
                TcpSessionOptions session_options;
                session_options.max_write_queue_depth =
                    options_.max_write_queue_depth;
                session_options.counters = counters;
                session_options.on_close =
                    [counters](TcpCloseReason) {
                        counters->active_connections.fetch_sub(
                            1,
                            std::memory_order_relaxed);
                        counters->closed_connections.fetch_add(
                            1,
                            std::memory_order_relaxed);
                    };
                auto session = std::make_shared<TcpSession>(
                    std::move(*socket),
                    std::move(session_options));
                auto handler = handler_;
                boost::asio::post(
                    *raw_session_context,
                    [handler = std::move(handler),
                     session = std::move(session)]() mutable {
                        handler(std::move(session));
                    });
                (*do_accept)();
            });
    };
    (*do_accept)();
    io_context_pool_.run();
    return 0;
}

TransportStats TcpListener::stats() const {
    return snapshot_transport_counters(*counters_);
}

}  // namespace runtime::net

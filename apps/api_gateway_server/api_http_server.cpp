#include "apps/api_gateway_server/api_http_server.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace apps::api_gateway_server {
namespace {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

constexpr std::size_t kMaxRequestBodyBytes = 1024 * 1024;
constexpr auto kHttpOperationTimeout = std::chrono::seconds(15);

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    HttpSession(tcp::socket socket, ApiHandler& handler)
        : stream_(std::move(socket)), handler_(handler) {}

    void run() {
        do_read();
    }

private:
    void do_read() {
        parser_.emplace();
        parser_->body_limit(kMaxRequestBodyBytes);
        stream_.expires_after(kHttpOperationTimeout);
        http::async_read(
            stream_,
            buffer_,
            *parser_,
            beast::bind_front_handler(
                &HttpSession::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code error, std::size_t /*bytes_transferred*/) {
        if (error == http::error::end_of_stream) {
            do_close();
            return;
        }
        if (error) {
            return;
        }

        auto response = std::make_shared<http::response<http::string_body>>(
            handler_.handle(parser_->release()));
        parser_.reset();
        const bool close = response->need_eof();
        stream_.expires_after(kHttpOperationTimeout);
        http::async_write(
            stream_,
            *response,
            beast::bind_front_handler(
                &HttpSession::on_write,
                shared_from_this(),
                close,
                response));
    }

    void on_write(
        bool close,
        std::shared_ptr<http::response<http::string_body>> /*response*/,
        beast::error_code error,
        std::size_t /*bytes_transferred*/) {
        if (error) {
            return;
        }
        if (close) {
            do_close();
            return;
        }
        do_read();
    }

    void do_close() {
        beast::error_code ignored;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ignored);
    }

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    ApiHandler& handler_;
    std::optional<http::request_parser<http::string_body>> parser_;
};

class HttpListener : public std::enable_shared_from_this<HttpListener> {
public:
    HttpListener(
        net::io_context& io_context,
        tcp::endpoint endpoint,
        ApiHandler& handler)
        : io_context_(io_context),
          acceptor_(net::make_strand(io_context)),
          handler_(handler) {
        beast::error_code error;
        acceptor_.open(endpoint.protocol(), error);
        if (error) {
            throw beast::system_error(error);
        }
        acceptor_.set_option(net::socket_base::reuse_address(true), error);
        if (error) {
            throw beast::system_error(error);
        }
        acceptor_.bind(endpoint, error);
        if (error) {
            throw beast::system_error(error);
        }
        acceptor_.listen(net::socket_base::max_listen_connections, error);
        if (error) {
            throw beast::system_error(error);
        }
    }

    void run() {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            net::make_strand(io_context_),
            beast::bind_front_handler(
                &HttpListener::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code error, tcp::socket socket) {
        if (!error) {
            std::make_shared<HttpSession>(std::move(socket), handler_)->run();
        }
        do_accept();
    }

    net::io_context& io_context_;
    tcp::acceptor acceptor_;
    ApiHandler& handler_;
};

}  // namespace

int run_http_server(int port, ApiHandler& handler, int thread_count) {
    const int bounded_thread_count = std::max(1, thread_count);
    net::io_context io_context{bounded_thread_count};
    std::make_shared<HttpListener>(
        io_context,
        tcp::endpoint(tcp::v4(), static_cast<unsigned short>(port)),
        handler)
        ->run();

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(bounded_thread_count - 1));
    for (int index = 1; index < bounded_thread_count; ++index) {
        threads.emplace_back([&io_context] {
            io_context.run();
        });
    }
    io_context.run();
    for (auto& thread : threads) {
        thread.join();
    }
    return 0;
}

}  // namespace apps::api_gateway_server

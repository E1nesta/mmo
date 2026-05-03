#include "apps/api_gateway_server/api_http_server.h"

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <exception>
#include <functional>
#include <thread>
#include <utility>

namespace mmo::apps::api_gateway_server {
namespace {

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

void handle_session(tcp::socket socket, ApiHandler& handler) {
    try {
        beast::flat_buffer buffer;
        for (;;) {
            http::request<http::string_body> request;
            http::read(socket, buffer, request);
            auto response = handler.handle(request);
            const bool close = response.need_eof();
            http::write(socket, response);
            if (close) {
                break;
            }
        }
        beast::error_code ignored;
        socket.shutdown(tcp::socket::shutdown_send, ignored);
    } catch (const std::exception&) {
    }
}

}  // namespace

int run_http_server(int port, ApiHandler& handler) {
    boost::asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));

    for (;;) {
        tcp::socket socket(io_context);
        acceptor.accept(socket);
        std::thread(handle_session, std::move(socket), std::ref(handler)).detach();
    }
    return 0;
}

}  // namespace mmo::apps::api_gateway_server

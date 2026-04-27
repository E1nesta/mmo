// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/http/http_server.h"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace framework::http {

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

struct HttpServer::Impl {
    asio::io_context io_context;
    std::unique_ptr<tcp::acceptor> acceptor;
};

namespace {

std::string ToString(beast::string_view value) {
    return std::string(value.data(), value.size());
}

std::string ToLower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

void HandleConnection(tcp::socket socket, HttpRouter* router) {
    beast::flat_buffer buffer;
    http::request<http::string_body> request;
    beast::error_code ec;
    http::read(socket, buffer, request, ec);
    if (ec) {
        return;
    }

    HttpRequest incoming;
    incoming.method = ToString(request.method_string());
    incoming.target = ToString(request.target());
    incoming.body = request.body();
    for (const auto& header : request.base()) {
        incoming.headers.emplace(ToLower(ToString(header.name_string())), ToString(header.value()));
    }

    const auto app_response = router != nullptr ? router->Handle(incoming) : HttpResponse{500, "text/plain", "router unavailable", {}};

    http::response<http::string_body> response{http::status(app_response.status), request.version()};
    response.set(http::field::server, "mobile_game_backend");
    response.set(http::field::content_type, app_response.content_type);
    for (const auto& [key, value] : app_response.headers) {
        response.set(key, value);
    }
    response.body() = app_response.body;
    response.keep_alive(false);
    response.prepare_payload();

    http::write(socket, response, ec);
    socket.shutdown(tcp::socket::shutdown_both, ec);
}

}  // namespace

HttpServer::HttpServer(std::string host, int port)
    : host_(std::move(host)),
      port_(port),
      impl_(std::make_unique<Impl>()) {}

HttpServer::~HttpServer() {
    Shutdown();
}

// 服务主流程：`Start` 串联启动、运行与收敛阶段。
bool HttpServer::Start(HttpRouter* router, std::string* error_message) {
    if (running_.load()) {
        return true;
    }
    router_ = router;

    beast::error_code ec;
    const auto address = asio::ip::make_address(host_, ec);
    if (ec) {
        if (error_message != nullptr) {
            *error_message = "invalid http listen host";
        }
        return false;
    }

    impl_->acceptor = std::make_unique<tcp::acceptor>(impl_->io_context);
    impl_->acceptor->open(address.is_v6() ? tcp::v6() : tcp::v4(), ec);
    if (ec) {
        if (error_message != nullptr) {
            *error_message = "failed to open http acceptor";
        }
        return false;
    }
    impl_->acceptor->set_option(asio::socket_base::reuse_address(true), ec);
    impl_->acceptor->bind({address, static_cast<unsigned short>(port_)}, ec);
    if (ec) {
        if (error_message != nullptr) {
            *error_message = "failed to bind http acceptor";
        }
        return false;
    }
    impl_->acceptor->listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
        if (error_message != nullptr) {
            *error_message = "failed to listen http acceptor";
        }
        return false;
    }

    running_.store(true);
    thread_ = std::make_unique<std::thread>([this] { Run(); });
    return true;
}

// 服务主流程：`Run` 串联启动、运行与收敛阶段。
void HttpServer::Run() {
    while (running_.load()) {
        beast::error_code ec;
        tcp::socket socket(impl_->io_context);
        impl_->acceptor->accept(socket, ec);
        if (ec) {
            if (!running_.load()) {
                break;
            }
            continue;
        }
        std::thread(HandleConnection, std::move(socket), router_).detach();
    }
}

void HttpServer::Wait() {
    if (thread_ != nullptr && thread_->joinable()) {
        thread_->join();
    }
}

// 服务主流程：`Shutdown` 串联启动、运行与收敛阶段。
void HttpServer::Shutdown() {
    if (!running_.exchange(false)) {
        return;
    }
    beast::error_code ec;
    if (impl_ != nullptr && impl_->acceptor != nullptr) {
        impl_->acceptor->cancel(ec);
        impl_->acceptor->close(ec);
    }
    if (thread_ != nullptr && thread_->joinable()) {
        thread_->join();
    }
    thread_.reset();
}

}  // namespace framework::http

#include "runtime/transport/tcp_envelope_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>

#include "runtime/protocol/envelope_utils.h"

namespace mmo::runtime::transport {
namespace {

bool read_exact(int fd, void* buffer, std::size_t size) {
    auto* out = static_cast<char*>(buffer);
    std::size_t read_total = 0;
    while (read_total < size) {
        const ssize_t received = ::recv(fd, out + read_total, size - read_total, 0);
        if (received <= 0) {
            return false;
        }
        read_total += static_cast<std::size_t>(received);
    }
    return true;
}

bool write_exact(int fd, const void* buffer, std::size_t size) {
    const auto* input = static_cast<const char*>(buffer);
    std::size_t written_total = 0;
    while (written_total < size) {
        const ssize_t sent = ::send(fd, input + written_total, size - written_total, 0);
        if (sent <= 0) {
            return false;
        }
        written_total += static_cast<std::size_t>(sent);
    }
    return true;
}

bool read_envelope(int fd, mmo::public_api::Envelope& envelope) {
    std::uint32_t network_size = 0;
    if (!read_exact(fd, &network_size, sizeof(network_size))) {
        return false;
    }

    const std::uint32_t payload_size = ntohl(network_size);
    std::string payload(payload_size, '\0');
    if (payload_size > 0 &&
        !read_exact(fd, payload.data(), static_cast<std::size_t>(payload_size))) {
        return false;
    }

    return envelope.ParseFromString(payload);
}

bool write_envelope(int fd, const mmo::public_api::Envelope& envelope) {
    std::string payload;
    envelope.SerializeToString(&payload);
    const std::uint32_t network_size =
        htonl(static_cast<std::uint32_t>(payload.size()));
    return write_exact(fd, &network_size, sizeof(network_size)) &&
           write_exact(fd, payload.data(), payload.size());
}

int make_server_socket(std::uint16_t port) {
    const int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        return -1;
    }

    int reuse = 1;
    ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (::bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(server_fd);
        return -1;
    }

    if (::listen(server_fd, 64) < 0) {
        ::close(server_fd);
        return -1;
    }

    return server_fd;
}

}  // namespace

TcpEnvelopeServer::TcpEnvelopeServer(std::uint16_t port, EnvelopeHandler handler)
    : port_(port), handler_(std::move(handler)) {}

int TcpEnvelopeServer::run() const {
    const int server_fd = make_server_socket(port_);
    if (server_fd < 0) {
        std::cerr << "failed to listen on port " << port_ << ": "
                  << std::strerror(errno) << '\n';
        return 1;
    }

    std::cout << "listening on 0.0.0.0:" << port_ << '\n';
    while (true) {
        sockaddr_in client_address{};
        socklen_t client_len = sizeof(client_address);
        const int client_fd =
            ::accept(server_fd, reinterpret_cast<sockaddr*>(&client_address), &client_len);
        if (client_fd < 0) {
            continue;
        }

        mmo::public_api::Envelope request;
        mmo::public_api::Envelope response;
        if (read_envelope(client_fd, request)) {
            response = handler_(request);
        } else {
            response = mmo::runtime::protocol::make_error_envelope(
                request, 400, "invalid envelope");
        }

        write_envelope(client_fd, response);
        ::close(client_fd);
    }
}

mmo::public_api::Envelope send_envelope(
    const std::string& host,
    std::uint16_t port,
    const mmo::public_api::Envelope& request) {
    const int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        return mmo::runtime::protocol::make_error_envelope(
            request, 500, "failed to create client socket");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        ::close(client_fd);
        return mmo::runtime::protocol::make_error_envelope(
            request, 500, "invalid upstream host");
    }

    if (::connect(client_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(client_fd);
        return mmo::runtime::protocol::make_error_envelope(
            request, 502, "failed to connect upstream");
    }

    if (!write_envelope(client_fd, request)) {
        ::close(client_fd);
        return mmo::runtime::protocol::make_error_envelope(
            request, 502, "failed to send upstream request");
    }

    mmo::public_api::Envelope response;
    if (!read_envelope(client_fd, response)) {
        ::close(client_fd);
        return mmo::runtime::protocol::make_error_envelope(
            request, 502, "failed to read upstream response");
    }

    ::close(client_fd);
    return response;
}

}  // namespace mmo::runtime::transport

#include "runtime/foundation/readiness.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace mmo::runtime::foundation {
namespace {

class SocketGuard {
public:
    explicit SocketGuard(int fd) : fd_(fd) {}
    SocketGuard(const SocketGuard&) = delete;
    SocketGuard& operator=(const SocketGuard&) = delete;
    ~SocketGuard() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    int get() const {
        return fd_;
    }

private:
    int fd_{-1};
};

std::string dependency_message(
    const std::string& dependency_name,
    const std::string& detail) {
    return dependency_name + " dependency is not reachable: " + detail;
}

}  // namespace

ReadinessResult make_ready_result() {
    ReadinessResult result;
    result.ready = true;
    result.error_code = "ready";
    result.message = "ready";
    return result;
}

ReadinessResult make_not_ready_result(
    std::string error_code,
    std::string message) {
    ReadinessResult result;
    result.ready = false;
    result.error_code = std::move(error_code);
    result.message = std::move(message);
    return result;
}

ReadinessResult check_tcp_dependency(
    const std::string& dependency_name,
    const std::string& host,
    std::uint16_t port,
    std::chrono::milliseconds timeout) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* raw_results = nullptr;
    const std::string port_text = std::to_string(port);
    const int resolve_status =
        getaddrinfo(host.c_str(), port_text.c_str(), &hints, &raw_results);
    if (resolve_status != 0) {
        return make_not_ready_result(
            "dependency_unreachable",
            dependency_message(dependency_name, gai_strerror(resolve_status)));
    }

    bool attempted = false;
    std::string last_error = "no address resolved";
    for (addrinfo* address = raw_results; address != nullptr;
         address = address->ai_next) {
        attempted = true;
        SocketGuard socket_fd(
            socket(address->ai_family, address->ai_socktype, address->ai_protocol));
        if (socket_fd.get() < 0) {
            last_error = std::strerror(errno);
            continue;
        }
        const int flags = fcntl(socket_fd.get(), F_GETFL, 0);
        if (flags < 0 ||
            fcntl(socket_fd.get(), F_SETFL, flags | O_NONBLOCK) != 0) {
            last_error = std::strerror(errno);
            continue;
        }

        pollfd write_probe{};
        write_probe.fd = socket_fd.get();
        write_probe.events = POLLOUT;

        const int connect_result =
            connect(socket_fd.get(), address->ai_addr, address->ai_addrlen);
        if (connect_result == 0) {
            freeaddrinfo(raw_results);
            return make_ready_result();
        }
        if (errno != EINPROGRESS) {
            last_error = std::strerror(errno);
            continue;
        }

        const int timeout_millis =
            timeout.count() > 0 ? static_cast<int>(timeout.count()) : 1;
        const int poll_result = poll(&write_probe, 1, timeout_millis);
        if (poll_result <= 0) {
            last_error = poll_result == 0 ? "timeout" : std::strerror(errno);
            continue;
        }

        int socket_error = 0;
        socklen_t socket_error_size = sizeof(socket_error);
        if (getsockopt(
                socket_fd.get(),
                SOL_SOCKET,
                SO_ERROR,
                &socket_error,
                &socket_error_size) != 0) {
            last_error = std::strerror(errno);
            continue;
        }
        if (socket_error == 0) {
            freeaddrinfo(raw_results);
            return make_ready_result();
        }
        last_error = std::strerror(socket_error);
    }

    freeaddrinfo(raw_results);
    if (!attempted) {
        last_error = "no address attempted";
    }
    return make_not_ready_result(
        "dependency_unreachable",
        dependency_message(dependency_name, last_error));
}

ReadinessResult check_tcp_dependency(
    const std::string& dependency_name,
    const ServiceConfig& service,
    std::chrono::milliseconds timeout) {
    return check_tcp_dependency(
        dependency_name, service.host, service.tcp_port, timeout);
}

}  // namespace mmo::runtime::foundation

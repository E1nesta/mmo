#include "runtime/protocol/packet_codec.h"
#include "runtime/transport/proxy_protocol.h"
#include "runtime/transport/transport_server.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

bool Expect(bool condition, const std::string& message) {
    if (condition) {
        return true;
    }

    std::cerr << message << '\n';
    return false;
}

int ReservePort() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(fd);
        return -1;
    }

    socklen_t address_length = sizeof(address);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&address), &address_length) != 0) {
        close(fd);
        return -1;
    }

    const auto port = ntohs(address.sin_port);
    close(fd);
    return port;
}

bool SendAll(int fd, std::string_view buffer) {
    const char* cursor = buffer.data();
    std::size_t remaining = buffer.size();
    while (remaining > 0) {
        const auto written = send(fd, cursor, remaining, 0);
        if (written <= 0) {
            return false;
        }
        cursor += written;
        remaining -= static_cast<std::size_t>(written);
    }
    return true;
}

bool RecvAll(int fd, void* buffer, std::size_t length) {
    auto* cursor = static_cast<char*>(buffer);
    std::size_t remaining = length;
    while (remaining > 0) {
        const auto received = recv(fd, cursor, remaining, 0);
        if (received <= 0) {
            return false;
        }
        cursor += received;
        remaining -= static_cast<std::size_t>(received);
    }
    return true;
}

struct TestServer {
    std::unique_ptr<framework::transport::TransportServer> server;
    std::atomic_bool running{true};
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    std::string observed_peer_address;
};

bool StartProxyEchoServer(int port, TestServer* instance, std::string* error_message) {
    if (instance == nullptr) {
        if (error_message != nullptr) {
            *error_message = "test server instance is null";
        }
        return false;
    }

    framework::transport::TransportServer::Options options;
    options.io_threads = 1;
    options.idle_timeout_ms = 1000;
    options.proxy_protocol_enabled = true;
    instance->server = std::make_unique<framework::transport::TransportServer>(options);
    instance->server->SetPacketHandler(
        [instance](const framework::transport::TransportInbound& inbound,
                   framework::transport::ResponseCallback respond) {
            {
                std::lock_guard lock(instance->mutex);
                instance->observed_peer_address = inbound.peer_address;
            }
            instance->cv.notify_all();
            respond(inbound.packet);
        });

    if (!instance->server->Start("127.0.0.1", port, error_message)) {
        return false;
    }

    instance->thread = std::thread([instance] {
        instance->server->Run([instance] { return instance->running.load(); });
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return true;
}

void StopProxyEchoServer(TestServer* instance) {
    if (instance == nullptr) {
        return;
    }

    instance->running.store(false);
    if (instance->server != nullptr) {
        instance->server->Stop();
    }
    if (instance->thread.joinable()) {
        instance->thread.join();
    }
}

common::net::Packet BuildRequestPacket() {
    common::net::Packet packet;
    packet.header.request_id = 99;
    packet.header.msg_id = 1001;
    packet.body = "proxy-protocol-payload";
    packet.header.body_len = static_cast<std::uint32_t>(packet.body.size());
    return packet;
}

bool SendProxyEchoRequest(int port,
                          std::string_view proxy_header,
                          const common::net::Packet& request,
                          common::net::Packet* response,
                          std::string* error_message) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        if (error_message != nullptr) {
            *error_message = "failed to create socket";
        }
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        if (error_message != nullptr) {
            *error_message = "failed to connect";
        }
        close(fd);
        return false;
    }

    const auto encoded = framework::protocol::EncodePacket(request);
    const bool sent = SendAll(fd, proxy_header) && SendAll(fd, encoded);
    if (!sent) {
        if (error_message != nullptr) {
            *error_message = "failed to send proxy test payload";
        }
        close(fd);
        return false;
    }

    std::array<char, framework::protocol::kPacketHeaderSize> raw_header{};
    if (!RecvAll(fd, raw_header.data(), raw_header.size())) {
        if (error_message != nullptr) {
            *error_message = "failed to read response header";
        }
        close(fd);
        return false;
    }

    common::net::PacketHeader response_header{};
    std::string decode_error;
    if (!framework::protocol::DecodeHeader(
            std::string_view(raw_header.data(), raw_header.size()), &response_header, &decode_error)) {
        if (error_message != nullptr) {
            *error_message = "failed to decode response header: " + decode_error;
        }
        close(fd);
        return false;
    }

    std::string response_body(response_header.body_len, '\0');
    if (response_header.body_len > 0 &&
        !RecvAll(fd, response_body.data(), response_body.size())) {
        if (error_message != nullptr) {
            *error_message = "failed to read response body";
        }
        close(fd);
        return false;
    }

    close(fd);
    if (response != nullptr) {
        response->header = response_header;
        response->body = std::move(response_body);
    }
    return true;
}

}  // namespace

int main() {
    std::string peer_address;
    std::string error_message;
    if (!Expect(
            framework::transport::ParseProxyProtocolHeader(
                "PROXY TCP4 198.51.100.10 10.0.0.2 42351 7000\r\n", &peer_address, &error_message),
            "expected TCP4 proxy header to parse: " + error_message)) {
        return 1;
    }
    if (!Expect(peer_address == "198.51.100.10:42351",
                "expected parsed TCP4 peer address to use the original client endpoint")) {
        return 1;
    }

    if (!Expect(
            framework::transport::ParseProxyProtocolHeader("PROXY UNKNOWN\r\n", &peer_address, &error_message),
            "expected UNKNOWN proxy header to parse")) {
        return 1;
    }
    if (!Expect(peer_address.empty(), "expected UNKNOWN proxy header to clear peer address")) {
        return 1;
    }

    if (!Expect(
            !framework::transport::ParseProxyProtocolHeader(
                "PROXY TCP4 198.51.100.10 10.0.0.2 70000 7000\r\n", &peer_address, &error_message),
            "expected invalid source port to fail proxy parsing")) {
        return 1;
    }

    if (!Expect(
            !framework::transport::ParseProxyProtocolHeader(
                "PROXY TCP4 ::1 10.0.0.2 1234 7000\r\n", &peer_address, &error_message),
            "expected address family mismatch to fail proxy parsing")) {
        return 1;
    }

    const auto port = ReservePort();
    if (!Expect(port > 0, "expected to reserve a TCP port")) {
        return 1;
    }

    TestServer server;
    error_message.clear();
    if (!Expect(StartProxyEchoServer(port, &server, &error_message),
                "expected proxy echo server to start: " + error_message)) {
        return 1;
    }

    const auto request = BuildRequestPacket();
    common::net::Packet response;
    error_message.clear();
    if (!Expect(
            SendProxyEchoRequest(port,
                                 "PROXY TCP4 203.0.113.77 10.0.0.5 45678 7000\r\n",
                                 request,
                                 &response,
                                 &error_message),
            "expected proxy echo request to succeed: " + error_message)) {
        StopProxyEchoServer(&server);
        return 1;
    }

    if (!Expect(response.header.request_id == request.header.request_id,
                "expected response request_id to round-trip through proxy transport")) {
        StopProxyEchoServer(&server);
        return 1;
    }
    if (!Expect(response.body == request.body,
                "expected response body to round-trip through proxy transport")) {
        StopProxyEchoServer(&server);
        return 1;
    }

    {
        std::unique_lock lock(server.mutex);
        const bool received = server.cv.wait_for(
            lock,
            std::chrono::seconds(1),
            [&server] { return !server.observed_peer_address.empty(); });
        if (!Expect(received, "expected transport handler to observe proxied peer address")) {
            StopProxyEchoServer(&server);
            return 1;
        }
        if (!Expect(server.observed_peer_address == "203.0.113.77:45678",
                    "expected transport inbound peer address to use the proxy source endpoint")) {
            StopProxyEchoServer(&server);
            return 1;
        }
    }

    StopProxyEchoServer(&server);
    return 0;
}

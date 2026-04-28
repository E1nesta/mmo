#include "runtime/transport/tcp_envelope_server.h"

#include <boost/asio.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <arpa/inet.h>

#include "runtime/observability/logging.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"

namespace mmo::runtime::transport {
namespace {

using boost::asio::ip::tcp;

bool read_exact(tcp::socket& socket, void* buffer, std::size_t size) {
    boost::system::error_code error;
    boost::asio::read(socket, boost::asio::buffer(buffer, size), error);
    return !error;
}

bool write_exact(tcp::socket& socket, const void* buffer, std::size_t size) {
    boost::system::error_code error;
    boost::asio::write(socket, boost::asio::buffer(buffer, size), error);
    return !error;
}

template <typename Reader>
bool read_envelope_impl(
    Reader& reader,
    mmo::common::Envelope& envelope,
    std::uint32_t max_payload_bytes) {
    std::uint32_t network_size = 0;
    if (!read_exact(reader, &network_size, sizeof(network_size))) {
        return false;
    }

    const std::uint32_t payload_size = ntohl(network_size);
    if (payload_size > max_payload_bytes) {
        return false;
    }

    std::string payload(payload_size, '\0');
    if (payload_size > 0 &&
        !read_exact(reader, payload.data(), static_cast<std::size_t>(payload_size))) {
        return false;
    }

    return envelope.ParseFromString(payload);
}

template <typename Writer>
bool write_envelope_impl(
    Writer& writer,
    const mmo::common::Envelope& envelope,
    std::uint32_t max_payload_bytes) {
    std::string payload;
    envelope.SerializeToString(&payload);
    if (payload.size() > max_payload_bytes) {
        return false;
    }

    const std::uint32_t network_size =
        htonl(static_cast<std::uint32_t>(payload.size()));
    return write_exact(writer, &network_size, sizeof(network_size)) &&
           write_exact(writer, payload.data(), payload.size());
}

}  // namespace

TcpEnvelopeServer::TcpEnvelopeServer(
    std::uint16_t port,
    EnvelopeHandler handler,
    std::string service_name,
    TransportOptions options)
    : port_(port),
      handler_(std::move(handler)),
      service_name_(std::move(service_name)),
      options_(options) {}

int TcpEnvelopeServer::run() {
    try {
        boost::asio::io_context io_context;
        tcp::acceptor acceptor(io_context);
        const tcp::endpoint endpoint(tcp::v4(), port_);
        acceptor.open(endpoint.protocol());
        acceptor.set_option(tcp::acceptor::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen(options_.listen_backlog);

        mmo::runtime::observability::log_info(
            mmo::runtime::observability::LogContext{service_name_},
            "tcp_server_listening port=" + std::to_string(port_));

        while (true) {
            tcp::socket socket(io_context);
            boost::system::error_code accept_error;
            acceptor.accept(socket, accept_error);
            if (accept_error) {
                mmo::runtime::observability::log_error(
                    mmo::runtime::observability::LogContext{service_name_},
                    "tcp_accept_failed error=" + accept_error.message());
                continue;
            }

            mmo::common::Envelope request;
            if (!read_envelope_impl(socket, request, options_.max_payload_bytes)) {
                mmo::runtime::observability::log_warn(
                    mmo::runtime::observability::LogContext{service_name_},
                    "tcp_invalid_envelope");
                continue;
            }

            auto log_context =
                mmo::runtime::observability::context_from_envelope(
                    service_name_, request);
            const auto started = std::chrono::steady_clock::now();
            mmo::runtime::observability::log_info(log_context, "request_received");

            const mmo::common::Envelope response = handler_(request);

            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started);
            log_context.latency_ms = elapsed.count();
            if (!write_envelope_impl(socket, response, options_.max_payload_bytes)) {
                mmo::runtime::observability::log_error(
                    log_context,
                    "response_write_failed");
                continue;
            }

            if (response.message_type() == mmo::runtime::protocol::kErrorResponse) {
                log_context.error_code = 1;
                mmo::runtime::observability::log_error(log_context, "request_failed");
            } else {
                mmo::runtime::observability::log_info(log_context, "request_handled");
            }
        }
    } catch (const std::exception& error) {
        mmo::runtime::observability::log_error(
            mmo::runtime::observability::LogContext{service_name_},
            std::string("tcp_server_fatal error=") + error.what());
        return 1;
    }
}

}  // namespace mmo::runtime::transport

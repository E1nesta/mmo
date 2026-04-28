#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <ikcp.h>

#include "runtime/transport/envelope_transport.h"

namespace mmo::runtime::transport {

struct KcpOptions {
    int nodelay{1};
    int interval_millis{20};
    int fast_resend{2};
    int disable_congestion_control{1};
    int send_window{128};
    int receive_window{128};
};

struct RealtimeMessage {
    std::uint64_t request_id{};
    std::int64_t player_id{};
    std::string message_type;
    std::vector<std::uint8_t> payload;
};

using KcpOutput = std::function<int(const char* data, int size)>;

class KcpSession {
public:
    KcpSession(std::uint32_t conversation_id, KcpOutput output, KcpOptions options = {});
    ~KcpSession();

    KcpSession(const KcpSession&) = delete;
    KcpSession& operator=(const KcpSession&) = delete;
    KcpSession(KcpSession&&) noexcept;
    KcpSession& operator=(KcpSession&&) noexcept;

    std::uint32_t conversation_id() const;
    int send_reliable(const std::vector<std::uint8_t>& payload);
    int input_packet(const char* data, int size);
    void update(std::uint32_t current_millis);
    std::uint32_t next_update_millis(std::uint32_t current_millis) const;

private:
    static int output_callback(const char* data, int size, ikcpcb* kcp, void* user);

    std::uint32_t conversation_id_{};
    KcpOutput output_;
    ikcpcb* kcp_{};
};

class UdpKcpEndpoint {
public:
    explicit UdpKcpEndpoint(std::uint16_t port, TransportOptions options = {});

    std::uint16_t port() const;
    const TransportOptions& options() const;
    bool accepts_message_type(const std::string& message_type) const;

private:
    std::uint16_t port_{};
    TransportOptions options_;
};

}  // namespace mmo::runtime::transport

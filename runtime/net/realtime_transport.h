#pragma once

#include <cstdint>
#include <vector>

#include "runtime/net/kcp_session.h"

namespace runtime::net {

struct RealtimeMessage {
    std::uint64_t session_id{};
    std::uint64_t route_key{};
    std::uint16_t message_id{};
    std::uint32_t sequence{};
    std::vector<std::uint8_t> payload;
};

class RealtimeTransport {
public:
    explicit RealtimeTransport(
        std::uint16_t port,
        KcpOptions options = {});

    std::uint16_t port() const;
    const KcpOptions& options() const;

private:
    std::uint16_t port_{};
    KcpOptions options_;
};

}  // namespace runtime::net

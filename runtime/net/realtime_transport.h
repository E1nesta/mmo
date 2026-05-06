#pragma once

#include <cstdint>
#include <vector>

#include "runtime/net/kcp_session.h"

namespace runtime::net {

struct RealtimeMessage {
    std::uint64_t session_id{};
    std::uint64_t route_key{};
    std::uint32_t message_id{};
    std::uint32_t sequence{};
    std::vector<std::uint8_t> payload;
};

class RealtimeTransport {
public:
    explicit RealtimeTransport(
        std::uint16_t port,
        KcpOptions options = {},
        std::vector<std::uint32_t> accepted_message_ids = {});

    std::uint16_t port() const;
    const KcpOptions& options() const;
    bool accepts_message_id(std::uint32_t message_id) const;

private:
    std::uint16_t port_{};
    KcpOptions options_;
    std::vector<std::uint32_t> accepted_message_ids_;
};

}  // namespace runtime::net

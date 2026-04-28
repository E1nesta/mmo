#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "runtime/transport/kcp_session.h"

namespace mmo::runtime::transport {

struct RealtimeMessage {
    std::uint64_t request_id{};
    std::int64_t player_id{};
    std::string message_type;
    std::vector<std::uint8_t> payload;
};

class RealtimeTransport {
public:
    explicit RealtimeTransport(std::uint16_t port, KcpOptions options = {});

    std::uint16_t port() const;
    const KcpOptions& options() const;
    bool accepts_message_type(const std::string& message_type) const;

private:
    std::uint16_t port_{};
    KcpOptions options_;
};

}  // namespace mmo::runtime::transport

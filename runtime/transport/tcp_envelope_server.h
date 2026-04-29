#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "runtime/execution/sharded_executor.h"
#include "runtime/observability/metrics.h"
#include "runtime/transport/envelope_transport.h"

namespace mmo::runtime::transport {

class TcpEnvelopeServer : public EnvelopeServer {
public:
    TcpEnvelopeServer(
        std::uint16_t port,
        EnvelopeHandler handler,
        std::string service_name,
        TransportOptions options = {},
        std::shared_ptr<mmo::runtime::execution::ShardedExecutor> handler_executor =
            nullptr,
        std::shared_ptr<mmo::runtime::observability::MetricsRegistry> metrics =
            nullptr);

    int run() override;

private:
    std::uint16_t port_{};
    EnvelopeHandler handler_;
    std::string service_name_;
    TransportOptions options_;
    std::shared_ptr<mmo::runtime::execution::ShardedExecutor> handler_executor_;
    std::shared_ptr<mmo::runtime::observability::MetricsRegistry> metrics_;
};

}  // namespace mmo::runtime::transport

#pragma once

#include <string>

#include "runtime/protocol/message_router.h"
#include "runtime/transport/envelope_transport.h"

namespace runtime::gateway {

class GatewayRouter {
public:
    using Handler = runtime::transport::EnvelopeHandler;

    void on(std::string message_type, Handler handler);
    mmo::common::Envelope dispatch(const mmo::common::Envelope& envelope) const;
    Handler handler() const;

private:
    runtime::protocol::MessageRouter router_;
};

}  // namespace runtime::gateway

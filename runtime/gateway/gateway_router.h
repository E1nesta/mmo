#pragma once

#include <string>

#include "runtime/protocol/message_router.h"
#include "runtime/transport/envelope_transport.h"

namespace mmo::runtime::gateway {

class GatewayRouter {
public:
    using Handler = mmo::runtime::transport::EnvelopeHandler;

    void on(std::string message_type, Handler handler);
    mmo::common::Envelope dispatch(const mmo::common::Envelope& envelope) const;
    Handler handler() const;

private:
    mmo::runtime::protocol::MessageRouter router_;
};

}  // namespace mmo::runtime::gateway

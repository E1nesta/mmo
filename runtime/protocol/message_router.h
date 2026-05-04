#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "runtime/transport/envelope_transport.h"

namespace runtime::protocol {

class MessageRouter {
public:
    using Handler = runtime::transport::EnvelopeHandler;

    void on(std::string message_type, Handler handler);
    mmo::common::Envelope dispatch(const mmo::common::Envelope& envelope) const;
    Handler handler() const;

private:
    std::unordered_map<std::string, Handler> handlers_;
};

}  // namespace runtime::protocol

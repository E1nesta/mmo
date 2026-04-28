#include "runtime/transport/envelope_transport.h"

namespace mmo::runtime::transport {
namespace {

static_assert(kDefaultMaxEnvelopePayloadBytes > 0);
static_assert(kDefaultTransportTimeoutMillis > 0);

}  // namespace

TransportOptions make_transport_options(
    const mmo::runtime::foundation::TcpTransportConfig& config) {
    TransportOptions options;
    options.max_payload_bytes = config.max_envelope_payload_bytes;
    options.timeout_millis = config.timeout_millis;
    options.listen_backlog = config.listen_backlog;
    return options;
}

}  // namespace mmo::runtime::transport

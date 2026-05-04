#include "runtime/transport/envelope_transport.h"

namespace runtime::transport {
namespace {

static_assert(kDefaultMaxEnvelopePayloadBytes > 0);
static_assert(kDefaultTransportTimeoutMillis > 0);

}  // namespace

TransportOptions make_transport_options(
    const runtime::foundation::TcpTransportConfig& config) {
    TransportOptions options;
    options.max_payload_bytes = config.max_envelope_payload_bytes;
    options.timeout_millis = config.timeout_millis;
    options.listen_backlog = config.listen_backlog;
    return options;
}

TransportOptions make_transport_options(
    const runtime::foundation::TcpTransportConfig& config,
    const runtime::foundation::ExecutionConfig& execution_config) {
    auto options = make_transport_options(config);
    options.io_thread_count = execution_config.io_threads;
    options.handler_shard_count = execution_config.handler_shards;
    options.max_handler_queue_depth_per_shard =
        static_cast<std::size_t>(
            execution_config.max_handler_queue_depth_per_shard);
    return options;
}

TransportEndpoint make_transport_endpoint(
    const runtime::foundation::ServiceConfig& config) {
    TransportEndpoint endpoint;
    endpoint.host = config.host;
    endpoint.port = config.tcp_port;
    return endpoint;
}

}  // namespace runtime::transport

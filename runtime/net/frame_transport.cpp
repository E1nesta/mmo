#include "runtime/net/frame_transport.h"

namespace runtime::net {

TransportOptions make_transport_options(
    const runtime::foundation::TcpTransportConfig& config) {
    TransportOptions options;
    options.max_payload_bytes = config.max_frame_payload_bytes > 0
        ? static_cast<std::uint32_t>(config.max_frame_payload_bytes)
        : kDefaultMaxFramePayloadBytes;
    options.timeout_millis = config.timeout_millis > 0
        ? config.timeout_millis
        : kDefaultTransportTimeoutMillis;
    options.listen_backlog = config.listen_backlog > 0
        ? config.listen_backlog
        : options.listen_backlog;
    return options;
}

TransportOptions make_transport_options(
    const runtime::foundation::TcpTransportConfig& config,
    const runtime::foundation::SchedulerConfig& scheduler_config) {
    auto options = make_transport_options(config);
    options.io_thread_count = scheduler_config.io_thread_count;
    options.handler_shard_count = scheduler_config.handler_shard_count;
    options.max_handler_queue_depth_per_shard =
        scheduler_config.max_task_queue_depth_per_shard;
    return options;
}

TransportEndpoint make_transport_endpoint(
    const runtime::foundation::ServiceConfig& config) {
    return TransportEndpoint{config.host, config.port};
}

}  // namespace runtime::net

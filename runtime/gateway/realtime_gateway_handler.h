#pragma once

#include <cstdint>

#include "runtime/gateway/realtime_gateway_route.h"
#include "runtime/net/realtime_packet_codec.h"
#include "runtime/observability/metrics.h"
#include "runtime/protocol/frame.h"
#include "runtime/rpc/rpc_client.h"
#include "runtime/rpc/rpc_error.h"
#include "runtime/rpc/rpc_options.h"
#include "runtime/session/session_context.h"

namespace runtime::gateway {

struct RealtimeGatewayContext {
    runtime::rpc::RpcClient& rpc_client;
    const RealtimeGatewayRouteTable& route_table;
    runtime::session::SessionRegistry& sessions;
    runtime::observability::MetricsRegistry& security_metrics;
};

struct RealtimeGatewayResult {
    bool accepted{};
    int error_code{};
};

std::uint64_t realtime_gateway_route_key(
    const RealtimeGatewayRoute& route,
    const runtime::net::RealtimePacket& packet,
    const runtime::session::SessionRegistry& sessions);

runtime::protocol::FrameMessage make_realtime_internal_frame(
    const RealtimeGatewayRoute& route,
    const runtime::net::RealtimePacket& packet,
    std::uint64_t route_key);

runtime::rpc::RpcOptions make_realtime_gateway_rpc_options(
    const RealtimeGatewayRoute& route,
    std::uint64_t route_key);

RealtimeGatewayResult handle_realtime_gateway_packet(
    RealtimeGatewayContext& context,
    const runtime::net::RealtimePacket& packet);

}  // namespace runtime::gateway

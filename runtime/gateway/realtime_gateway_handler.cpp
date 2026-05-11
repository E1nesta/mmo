#include "runtime/gateway/realtime_gateway_handler.h"

#include <utility>

#include "runtime/protocol/payload_utils.h"

namespace runtime::gateway {

std::uint64_t realtime_gateway_route_key(
    const RealtimeGatewayRoute& route,
    const runtime::net::RealtimePacket& packet,
    const runtime::session::SessionRegistry& sessions) {
    switch (route.route_key_policy) {
        case RealtimeRouteKeyPolicy::kRealtimeSessionId:
            return packet.realtime_session_id;
        case RealtimeRouteKeyPolicy::kNoRouteKey:
            return 0;
        case RealtimeRouteKeyPolicy::kPlayerId: {
            const auto binding =
                sessions.find_by_connection_id(packet.realtime_session_id);
            return binding.has_value() && binding->player_id > 0
                ? static_cast<std::uint64_t>(binding->player_id)
                : 0;
        }
    }
    return packet.realtime_session_id;
}

runtime::protocol::FrameMessage make_realtime_internal_frame(
    const RealtimeGatewayRoute& route,
    const runtime::net::RealtimePacket& packet,
    std::uint64_t route_key) {
    runtime::protocol::FrameMessage internal;
    internal.header.message_id = route.target_message_id;
    internal.header.request_id = packet.sequence;
    internal.header.route_key = route_key;
    internal.header.mode = route.mode;
    internal.header.flags = packet.flags;
    internal.payload.assign(packet.payload.begin(), packet.payload.end());
    return internal;
}

runtime::rpc::RpcOptions make_realtime_gateway_rpc_options(
    const RealtimeGatewayRoute& route,
    std::uint64_t route_key) {
    runtime::rpc::RpcOptions options;
    options.mode = route.mode;
    options.routing_policy = route.routing_policy;
    options.route_key = route_key;
    options.target_instance_id = route.target_instance_id;
    return options;
}

RealtimeGatewayResult handle_realtime_gateway_packet(
    RealtimeGatewayContext& context,
    const runtime::net::RealtimePacket& packet) {
    const auto route = context.route_table.find(packet.message_id);
    if (!route.has_value()) {
        return RealtimeGatewayResult{false, 404};
    }

    const auto binding =
        context.sessions.find_by_connection_id(packet.realtime_session_id);
    const auto now_millis =
        static_cast<std::uint64_t>(runtime::protocol::current_time_millis());
    if (!binding.has_value() || !binding->valid(now_millis)) {
        context.security_metrics.record_game_session_expired();
        return RealtimeGatewayResult{false, 401};
    }

    const auto route_key =
        realtime_gateway_route_key(*route, packet, context.sessions);
    if (route->route_key_policy == RealtimeRouteKeyPolicy::kPlayerId &&
        route_key == 0) {
        return RealtimeGatewayResult{false, 401};
    }

    auto internal_request =
        make_realtime_internal_frame(*route, packet, route_key);
    auto rpc_options = make_realtime_gateway_rpc_options(*route, route_key);
    const auto rpc_result = context.rpc_client.cast_frame(
        route->target_service,
        std::move(internal_request),
        std::move(rpc_options));
    if (!rpc_result.ok()) {
        return RealtimeGatewayResult{
            false,
            runtime::rpc::rpc_error_to_status_code(rpc_result.error().code)};
    }
    return RealtimeGatewayResult{true, 0};
}

}  // namespace runtime::gateway

#include "apps/realtime_gateway_server/realtime_route_table.h"

#include "proto/message_catalog.h"
#include "runtime/protocol/message_mode.h"
#include "runtime/rpc/rpc_routing_policy.h"

namespace apps::realtime_gateway_server {
namespace {

std::uint16_t public_message_id(std::uint32_t message_id) {
    return static_cast<std::uint16_t>(message_id);
}

}  // namespace

runtime::gateway::RealtimeGatewayRouteTable make_realtime_route_table() {
    runtime::gateway::RealtimeGatewayRouteTable route_table;
    route_table.add(
        public_message_id(mmo::protocol::kMoveCommand),
        runtime::gateway::RealtimeGatewayRoute{
            public_message_id(mmo::protocol::kMoveCommand),
            "scene_server",
            mmo::protocol::kMoveCommand,
            runtime::protocol::MessageMode::kCast,
            runtime::rpc::RpcRoutingPolicy::kStickyRouteKey,
            runtime::gateway::RealtimeRouteKeyPolicy::kPlayerId,
            ""});
    route_table.add(
        public_message_id(mmo::protocol::kCastSkillRequest),
        runtime::gateway::RealtimeGatewayRoute{
            public_message_id(mmo::protocol::kCastSkillRequest),
            "scene_server",
            mmo::protocol::kCastSkillRequest,
            runtime::protocol::MessageMode::kCast,
            runtime::rpc::RpcRoutingPolicy::kStickyRouteKey,
            runtime::gateway::RealtimeRouteKeyPolicy::kPlayerId,
            ""});
    return route_table;
}

}  // namespace apps::realtime_gateway_server

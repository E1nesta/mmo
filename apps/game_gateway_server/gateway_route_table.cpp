#include "apps/game_gateway_server/gateway_route_table.h"

#include "proto/message_catalog.h"
#include "runtime/protocol/message_mode.h"
#include "runtime/rpc/rpc_routing_policy.h"

namespace apps::game_gateway_server {

namespace {

std::uint16_t public_message_id(std::uint32_t message_id) {
    return static_cast<std::uint16_t>(message_id);
}

}  // namespace

runtime::gateway::GatewayRouteTable make_gateway_route_table() {
    runtime::gateway::GatewayRouteTable route_table;
    route_table.add(
        public_message_id(mmo::protocol::kEnterWorldRequest),
        runtime::gateway::GatewayRoute{
            public_message_id(mmo::protocol::kEnterWorldRequest),
            "world_server",
            mmo::protocol::kEnterWorldRequest,
            public_message_id(mmo::protocol::kEnterWorldResponse),
            runtime::protocol::MessageMode::kCall,
            true,
            runtime::rpc::RpcRoutingPolicy::kStickyRouteKey,
            runtime::gateway::GatewayRouteKeyPolicy::kPlayerId,
            ""});
    route_table.add(
        public_message_id(mmo::protocol::kEnterInstanceRequest),
        runtime::gateway::GatewayRoute{
            public_message_id(mmo::protocol::kEnterInstanceRequest),
            "instance_server",
            mmo::protocol::kEnterInstanceRequest,
            public_message_id(mmo::protocol::kEnterInstanceResponse),
            runtime::protocol::MessageMode::kCall,
            true,
            runtime::rpc::RpcRoutingPolicy::kStickyRouteKey,
            runtime::gateway::GatewayRouteKeyPolicy::kPlayerId,
            ""});
    route_table.add(
        public_message_id(mmo::protocol::kSettleInstanceRequest),
        runtime::gateway::GatewayRoute{
            public_message_id(mmo::protocol::kSettleInstanceRequest),
            "instance_server",
            mmo::protocol::kSettleInstanceRequest,
            public_message_id(mmo::protocol::kSettleInstanceResponse),
            runtime::protocol::MessageMode::kCall,
            true,
            runtime::rpc::RpcRoutingPolicy::kStickyRouteKey,
            runtime::gateway::GatewayRouteKeyPolicy::kPlayerId,
            ""});
    route_table.add(
        public_message_id(mmo::protocol::kApplyRewardRequest),
        runtime::gateway::GatewayRoute{
            public_message_id(mmo::protocol::kApplyRewardRequest),
            "player_server",
            mmo::protocol::kApplyRewardRequest,
            public_message_id(mmo::protocol::kApplyRewardResponse),
            runtime::protocol::MessageMode::kCall,
            true,
            runtime::rpc::RpcRoutingPolicy::kStickyRouteKey,
            runtime::gateway::GatewayRouteKeyPolicy::kPlayerId,
            ""});
    route_table.add(
        public_message_id(mmo::protocol::kSocialBoundaryRequest),
        runtime::gateway::GatewayRoute{
            public_message_id(mmo::protocol::kSocialBoundaryRequest),
            "social_server",
            mmo::protocol::kSocialBoundaryRequest,
            public_message_id(mmo::protocol::kSocialBoundaryResponse),
            runtime::protocol::MessageMode::kCall,
            true,
            runtime::rpc::RpcRoutingPolicy::kStickyRouteKey,
            runtime::gateway::GatewayRouteKeyPolicy::kPlayerId,
            ""});
    return route_table;
}

}  // namespace apps::game_gateway_server

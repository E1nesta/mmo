#include "apps/game_gateway_server/gateway_route_table.h"

#include "runtime/channel/routing_policy.h"
#include "runtime/protocol/message_types.h"

namespace mmo::apps::game_gateway_server {

mmo::runtime::gateway::ProxyRouteTable make_gateway_route_table() {
    mmo::runtime::gateway::ProxyRouteTable route_table;
    route_table.add(
        mmo::runtime::protocol::kLoginRequest,
        mmo::runtime::gateway::ProxyRoute{
            "auth_server",
            mmo::runtime::protocol::kGatewayAuthLoginRequest,
            false,
            mmo::runtime::channel::RoutingPolicy::kLeastPending,
            "",
            ""});
    route_table.add(
        mmo::runtime::protocol::kEnterWorldRequest,
        mmo::runtime::gateway::ProxyRoute{
            "world_server",
            mmo::runtime::protocol::kGatewayEnterWorldRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::runtime::protocol::kEnterInstanceRequest,
        mmo::runtime::gateway::ProxyRoute{
            "instance_server",
            mmo::runtime::protocol::kGatewayEnterInstanceRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::runtime::protocol::kSettleInstanceRequest,
        mmo::runtime::gateway::ProxyRoute{
            "instance_server",
            mmo::runtime::protocol::kGatewaySettleInstanceRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyInstance,
            "",
            ""});
    route_table.add(
        mmo::runtime::protocol::kApplyRewardRequest,
        mmo::runtime::gateway::ProxyRoute{
            "player_server",
            mmo::runtime::protocol::kGatewayApplyRewardRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::runtime::protocol::kSocialBoundaryRequest,
        mmo::runtime::gateway::ProxyRoute{
            "social_server",
            mmo::runtime::protocol::kGatewaySocialBoundaryRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    return route_table;
}

}  // namespace mmo::apps::game_gateway_server

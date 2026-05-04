#include "apps/game_gateway_server/gateway_route_table.h"

#include "runtime/channel/routing_policy.h"
#include "apps/protocol/message_types.h"

namespace mmo::apps::game_gateway_server {

mmo::runtime::gateway::ProxyRouteTable make_gateway_route_table() {
    mmo::runtime::gateway::ProxyRouteTable route_table;
    route_table.add(
        mmo::apps::protocol::kLoginRequest,
        mmo::runtime::gateway::ProxyRoute{
            "auth_server",
            mmo::apps::protocol::kGatewayAuthLoginRequest,
            false,
            mmo::runtime::channel::RoutingPolicy::kLeastPending,
            "",
            ""});
    route_table.add(
        mmo::apps::protocol::kEnterWorldRequest,
        mmo::runtime::gateway::ProxyRoute{
            "world_server",
            mmo::apps::protocol::kGatewayEnterWorldRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::apps::protocol::kEnterInstanceRequest,
        mmo::runtime::gateway::ProxyRoute{
            "instance_server",
            mmo::apps::protocol::kGatewayEnterInstanceRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::apps::protocol::kSettleInstanceRequest,
        mmo::runtime::gateway::ProxyRoute{
            "instance_server",
            mmo::apps::protocol::kGatewaySettleInstanceRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyInstance,
            "",
            ""});
    route_table.add(
        mmo::apps::protocol::kApplyRewardRequest,
        mmo::runtime::gateway::ProxyRoute{
            "player_server",
            mmo::apps::protocol::kGatewayApplyRewardRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::apps::protocol::kSocialBoundaryRequest,
        mmo::runtime::gateway::ProxyRoute{
            "social_server",
            mmo::apps::protocol::kGatewaySocialBoundaryRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    return route_table;
}

}  // namespace mmo::apps::game_gateway_server

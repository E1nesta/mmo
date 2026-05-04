#include "apps/game_gateway_server/gateway_route_table.h"

#include "runtime/channel/routing_policy.h"
#include "apps/protocol/message_types.h"

namespace apps::game_gateway_server {

runtime::gateway::ProxyRouteTable make_gateway_route_table() {
    runtime::gateway::ProxyRouteTable route_table;
    route_table.add(
        apps::protocol::kLoginRequest,
        runtime::gateway::ProxyRoute{
            "auth_server",
            apps::protocol::kGatewayAuthLoginRequest,
            false,
            runtime::channel::RoutingPolicy::kLeastPending,
            "",
            ""});
    route_table.add(
        apps::protocol::kEnterWorldRequest,
        runtime::gateway::ProxyRoute{
            "world_server",
            apps::protocol::kGatewayEnterWorldRequest,
            true,
            runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        apps::protocol::kEnterInstanceRequest,
        runtime::gateway::ProxyRoute{
            "instance_server",
            apps::protocol::kGatewayEnterInstanceRequest,
            true,
            runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        apps::protocol::kSettleInstanceRequest,
        runtime::gateway::ProxyRoute{
            "instance_server",
            apps::protocol::kGatewaySettleInstanceRequest,
            true,
            runtime::channel::RoutingPolicy::kStickyInstance,
            "",
            ""});
    route_table.add(
        apps::protocol::kApplyRewardRequest,
        runtime::gateway::ProxyRoute{
            "player_server",
            apps::protocol::kGatewayApplyRewardRequest,
            true,
            runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        apps::protocol::kSocialBoundaryRequest,
        runtime::gateway::ProxyRoute{
            "social_server",
            apps::protocol::kGatewaySocialBoundaryRequest,
            true,
            runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    return route_table;
}

}  // namespace apps::game_gateway_server

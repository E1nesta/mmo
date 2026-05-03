#include "apps/game_gateway_server/gateway_route_table.h"

#include "runtime/channel/routing_policy.h"
#include "runtime/protocol/message_types.h"

namespace mmo::apps::game_gateway_server {

mmo::runtime::routing::RouteTable make_gateway_route_table() {
    mmo::runtime::routing::RouteTable route_table;
    route_table.add(
        mmo::runtime::protocol::kLoginRequest,
        mmo::runtime::routing::RouteTarget{
            "auth_server",
            mmo::runtime::protocol::kGatewayAuthLoginRequest,
            false,
            mmo::runtime::channel::RoutingPolicy::kLeastPending,
            "",
            ""});
    route_table.add(
        mmo::runtime::protocol::kEnterWorldRequest,
        mmo::runtime::routing::RouteTarget{
            "world_server",
            mmo::runtime::protocol::kGatewayEnterWorldRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::runtime::protocol::kEnterInstanceRequest,
        mmo::runtime::routing::RouteTarget{
            "instance_server",
            mmo::runtime::protocol::kGatewayEnterInstanceRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::runtime::protocol::kSettleInstanceRequest,
        mmo::runtime::routing::RouteTarget{
            "instance_server",
            mmo::runtime::protocol::kGatewaySettleInstanceRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyInstance,
            "",
            ""});
    route_table.add(
        mmo::runtime::protocol::kApplyRewardRequest,
        mmo::runtime::routing::RouteTarget{
            "player_server",
            mmo::runtime::protocol::kGatewayApplyRewardRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::runtime::protocol::kSocialBoundaryRequest,
        mmo::runtime::routing::RouteTarget{
            "social_server",
            mmo::runtime::protocol::kGatewaySocialBoundaryRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    return route_table;
}

}  // namespace mmo::apps::game_gateway_server

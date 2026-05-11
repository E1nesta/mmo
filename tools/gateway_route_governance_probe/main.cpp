#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "runtime/gateway/gateway_context.h"
#include "runtime/gateway/gateway_middleware.h"
#include "runtime/gateway/gateway_route_handler.h"
#include "runtime/gateway/gateway_route_table.h"
#include "runtime/net/reliable_frame_codec.h"
#include "runtime/protocol/frame.h"
#include "runtime/protocol/message_mode.h"
#include "runtime/rpc/rpc_routing_policy.h"
#include "runtime/session/session_context.h"

namespace {

std::uint64_t now_millis() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

runtime::gateway::GatewayRoute make_player_route() {
    runtime::gateway::GatewayRoute route;
    route.target_service = "world_server";
    route.target_message_id = 30100;
    route.public_response_message_id = 10101;
    route.mode = runtime::protocol::MessageMode::kCall;
    route.require_session = true;
    route.routing_policy = runtime::rpc::RpcRoutingPolicy::kStickyRouteKey;
    route.route_key_policy = runtime::gateway::GatewayRouteKeyPolicy::kPlayerId;
    return route;
}

runtime::net::ReliableFrame make_reliable_frame() {
    runtime::net::ReliableFrame frame;
    frame.version = runtime::protocol::kProtocolVersion;
    frame.message_id = 10100;
    frame.flags = 0x02;
    frame.request_id = 77;
    frame.session_id = 0;
    frame.payload = {'p', 'a', 'y', 'l', 'o', 'a', 'd'};
    return frame;
}

void verify_route_table_validation() {
    runtime::gateway::GatewayRouteTable routes;
    routes.add(10100, make_player_route());

    assert(routes.size() == 1);
    assert(routes.contains(10100));
    const auto route = routes.find(10100);
    assert(route.has_value());
    assert(route->public_message_id == 10100);
    assert(route->target_service == "world_server");
    assert(route->target_message_id == 30100);
    assert(route->routing_policy ==
           runtime::rpc::RpcRoutingPolicy::kStickyRouteKey);
    assert(route->route_key_policy ==
           runtime::gateway::GatewayRouteKeyPolicy::kPlayerId);

    bool duplicate_rejected = false;
    try {
        routes.add(10100, make_player_route());
    } catch (const std::runtime_error&) {
        duplicate_rejected = true;
    }
    assert(duplicate_rejected);

    bool empty_service_rejected = false;
    try {
        auto invalid = make_player_route();
        invalid.target_service.clear();
        routes.add(10102, invalid);
    } catch (const std::runtime_error&) {
        empty_service_rejected = true;
    }
    assert(empty_service_rejected);

    bool zero_target_message_rejected = false;
    try {
        auto invalid = make_player_route();
        invalid.target_message_id = 0;
        routes.add(10103, invalid);
    } catch (const std::runtime_error&) {
        zero_target_message_rejected = true;
    }
    assert(zero_target_message_rejected);
}

void verify_shallow_internal_frame_wrap() {
    auto route = make_player_route();
    auto public_frame = make_reliable_frame();
    const std::uint64_t route_key = 1198216;

    const auto internal =
        runtime::gateway::make_internal_frame(route, public_frame, route_key);

    assert(internal.header.message_id == route.target_message_id);
    assert(internal.header.request_id == public_frame.request_id);
    assert(internal.header.route_key == route_key);
    assert(internal.header.mode == route.mode);
    assert(internal.header.flags == public_frame.flags);
    assert(internal.payload ==
           std::string(public_frame.payload.begin(), public_frame.payload.end()));
}

void verify_player_route_key_from_session() {
    runtime::session::SessionRegistry sessions;
    auto route = make_player_route();
    auto frame = make_reliable_frame();
    const auto now = now_millis();
    const auto binding = sessions.bind(
        1098216,
        1198216,
        "session-token",
        "game-gateway-1",
        "device-1",
        now,
        now + 60000,
        30000);
    frame.session_id = binding.connection_id;

    assert(runtime::gateway::gateway_route_key(route, frame, sessions) ==
           static_cast<std::uint64_t>(1198216));

    frame.session_id = 99999999;
    assert(runtime::gateway::gateway_route_key(route, frame, sessions) == 0);
}

void verify_session_guard() {
    runtime::session::SessionRegistry sessions;
    auto frame = make_reliable_frame();

    assert(runtime::gateway::validate_gateway_session(frame, sessions)
               .has_value());

    const auto now = now_millis();
    const auto binding = sessions.bind(
        1098216,
        1198216,
        "session-token",
        "game-gateway-1",
        "device-1",
        now,
        now + 60000,
        30000);
    frame.session_id = binding.connection_id;
    assert(!runtime::gateway::validate_gateway_session(frame, sessions)
                .has_value());
}

void verify_gateway_rpc_options() {
    auto route = make_player_route();
    route.target_instance_id = "world-server-1";
    const auto options = runtime::gateway::make_gateway_rpc_options(
        route, 1198216);

    assert(options.mode == runtime::protocol::MessageMode::kCall);
    assert(options.route_key == 1198216);
    assert(options.routing_policy ==
           runtime::rpc::RpcRoutingPolicy::kStickyRouteKey);
    assert(options.target_instance_id == "world-server-1");
}

}  // namespace

int main() {
    verify_route_table_validation();
    verify_shallow_internal_frame_wrap();
    verify_player_route_key_from_session();
    verify_session_guard();
    verify_gateway_rpc_options();

    std::cout << "gateway route governance probe ok\n";
    return 0;
}

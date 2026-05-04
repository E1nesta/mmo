#include "runtime/gateway/proxy_context.h"

namespace runtime::gateway {

runtime::channel::ChannelCallOptions make_proxy_call_options(
    const ProxyRoute& route,
    const mmo::common::RequestContext& context,
    const std::string& route_key) {
    runtime::channel::ChannelCallOptions options;
    options.routing_policy = route.routing_policy;
    options.target_instance_id = route.target_instance_id;
    if (!route_key.empty()) {
        options.route_key = route_key;
    } else if (route.route_key_source == "player_id") {
        options.route_key = std::to_string(context.player_id());
    } else if (route.route_key_source == "game_session_id") {
        options.route_key = context.game_session_id();
    }
    return options;
}

}  // namespace runtime::gateway

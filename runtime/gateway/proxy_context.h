#pragma once

#include <string>

#include "common/context.pb.h"
#include "runtime/channel/channel_call_options.h"
#include "runtime/observability/metrics.h"
#include "runtime/gateway/gateway_forwarder.h"
#include "runtime/gateway/gateway_router.h"
#include "runtime/gateway/proxy_route.h"
#include "runtime/session/session_context.h"
#include "runtime/session/session_store.h"

namespace mmo::runtime::gateway {

struct ProxyContext {
    GatewayForwarder& forwarder;
    const ProxyRouteTable& route_table;
    mmo::runtime::session::SessionRegistry& sessions;
    mmo::runtime::session::SessionStore& session_store;
    mmo::runtime::observability::MetricsRegistry& security_metrics;
};

mmo::runtime::channel::ChannelCallOptions make_proxy_call_options(
    const ProxyRoute& route,
    const mmo::common::RequestContext& context,
    const std::string& route_key = {});

}  // namespace mmo::runtime::gateway

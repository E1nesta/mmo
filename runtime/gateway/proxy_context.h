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

namespace runtime::gateway {

struct ProxyContext {
    GatewayForwarder& forwarder;
    const ProxyRouteTable& route_table;
    runtime::session::SessionRegistry& sessions;
    runtime::session::SessionStore& session_store;
    runtime::observability::MetricsRegistry& security_metrics;
};

runtime::channel::ChannelCallOptions make_proxy_call_options(
    const ProxyRoute& route,
    const mmo::common::RequestContext& context,
    const std::string& route_key = {});

}  // namespace runtime::gateway

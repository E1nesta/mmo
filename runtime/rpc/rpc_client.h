#pragma once

#include <memory>
#include <string>
#include <utility>

#include <google/protobuf/message.h>

#include "common/context.pb.h"
#include "runtime/channel/channel_client.h"
#include "runtime/channel/endpoint_resolver.h"
#include "runtime/channel/service_registry.h"
#include "runtime/foundation/server_config.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/rpc/rpc_controller.h"
#include "runtime/rpc/rpc_result.h"

namespace mmo::runtime::rpc {

struct RpcClientOptions {
    std::string source_service;
    std::string internal_auth_shared_secret;
};

RpcClientOptions make_rpc_client_options(
    const std::string& source_service,
    const mmo::runtime::foundation::ServerConfig& config);

class RpcClient {
public:
    RpcClient(
        std::shared_ptr<mmo::runtime::channel::EndpointResolver> resolver,
        mmo::runtime::transport::TransportOptions transport_options,
        mmo::runtime::channel::ChannelConnectionPoolOptions pool_options,
        RpcClientOptions options = {});
    RpcClient(
        std::shared_ptr<mmo::runtime::channel::ServiceRegistry> service_registry,
        mmo::runtime::transport::TransportOptions transport_options,
        mmo::runtime::channel::ChannelConnectionPoolOptions pool_options,
        RpcClientOptions options = {});
    explicit RpcClient(
        std::unique_ptr<mmo::runtime::channel::ChannelClient> channel_client,
        RpcClientOptions options = {});
    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;
    RpcClient(RpcClient&&) = delete;
    RpcClient& operator=(RpcClient&&) = delete;

    RpcResult call_envelope(
        const std::string& target_service,
        const mmo::common::Envelope& request,
        RpcController controller = {});

    template <typename Request>
    RpcResult call(
        const std::string& target_service,
        const std::string& message_type,
        const mmo::common::RequestContext& context,
        const Request& request,
        RpcController controller = {}) {
        controller.target_service = target_service;
        auto envelope =
            mmo::runtime::protocol::pack_message(message_type, context, request);
        return call_envelope(target_service, envelope, std::move(controller));
    }

private:
    std::unique_ptr<mmo::runtime::channel::ChannelClient> channel_client_;
    RpcClientOptions options_;
};

}  // namespace mmo::runtime::rpc

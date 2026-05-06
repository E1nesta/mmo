#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "runtime/foundation/server_config.h"
#include "runtime/net/frame_transport.h"
#include "runtime/protocol/payload_utils.h"
#include "runtime/rpc/rpc_connection_pool.h"
#include "runtime/rpc/rpc_options.h"
#include "runtime/rpc/rpc_result.h"
#include "runtime/rpc/rpc_service_registry.h"

namespace runtime::rpc {

struct RpcClientOptions {
    std::string source_service;
};

RpcClientOptions make_rpc_client_options(
    const std::string& source_service,
    const runtime::foundation::ServerConfig& config);

class RpcClient {
public:
    RpcClient(
        std::shared_ptr<runtime::rpc::RpcServiceRegistry> service_registry,
        runtime::net::TransportOptions transport_options,
        runtime::rpc::RpcConnectionPoolOptions pool_options,
        RpcClientOptions options = {});

    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;
    RpcClient(RpcClient&&) = delete;
    RpcClient& operator=(RpcClient&&) = delete;

    RpcResult call_frame(
        const std::string& target_service,
        runtime::protocol::FrameMessage request,
        RpcOptions options = {});
    RpcResult cast_frame(
        const std::string& target_service,
        runtime::protocol::FrameMessage request,
        RpcOptions options = {});

    template <typename Request>
    RpcResult call(
        const std::string& target_service,
        std::uint32_t message_id,
        std::uint64_t route_key,
        const Request& request,
        RpcOptions options = {}) {
        options.target_service = target_service;
        options.route_key = route_key;
        options.mode = runtime::protocol::MessageMode::kCall;
        auto frame = runtime::protocol::pack_message(
            message_id,
            request_id_or_next(options),
            route_key,
            options.mode,
            request);
        return call_frame(target_service, std::move(frame), std::move(options));
    }

    template <typename Event>
    RpcResult cast(
        const std::string& target_service,
        std::uint32_t message_id,
        std::uint64_t route_key,
        const Event& event,
        RpcOptions options = {}) {
        options.target_service = target_service;
        options.route_key = route_key;
        options.mode = runtime::protocol::MessageMode::kCast;
        auto frame = runtime::protocol::pack_message(
            message_id,
            request_id_or_next(options),
            route_key,
            options.mode,
            event);
        return cast_frame(target_service, std::move(frame), std::move(options));
    }

    template <typename Event>
    RpcResult batch(
        const std::string& target_service,
        std::uint32_t message_id,
        std::uint64_t route_key,
        const std::vector<Event>& events,
        RpcOptions options = {}) {
        options.target_service = target_service;
        options.route_key = route_key;
        options.mode = runtime::protocol::MessageMode::kBatch;
        auto frame = runtime::protocol::pack_batch(
            message_id,
            request_id_or_next(options),
            route_key,
            events);
        return cast_frame(target_service, std::move(frame), std::move(options));
    }

private:
    std::uint64_t request_id_or_next(const RpcOptions& options);
    RpcCallOptions make_call_options(const RpcOptions& options) const;

    RpcConnectionPool connection_pool_;
    RpcClientOptions options_;
    std::atomic<std::uint64_t> next_request_id_{1};
};

}  // namespace runtime::rpc

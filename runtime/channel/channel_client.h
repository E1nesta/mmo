#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include <google/protobuf/message.h>

#include "common/context.pb.h"
#include "runtime/channel/channel_call_options.h"
#include "runtime/channel/channel_connection_pool.h"
#include "runtime/channel/channel_result.h"
#include "runtime/protocol/envelope_utils.h"

namespace mmo::runtime::channel {

class ChannelClient {
public:
    virtual ~ChannelClient() = default;

    virtual ChannelResult call_envelope(
        const std::string& target_service,
        const mmo::common::Envelope& request,
        ChannelCallOptions options = {}) = 0;
};

class TcpChannelClient final : public ChannelClient {
public:
    TcpChannelClient(
        std::shared_ptr<EndpointResolver> resolver,
        mmo::runtime::transport::TransportOptions transport_options,
        ChannelConnectionPoolOptions pool_options);

    ChannelResult call_envelope(
        const std::string& target_service,
        const mmo::common::Envelope& request,
        ChannelCallOptions options = {}) override;

    template <typename Request>
    ChannelResult call(
        const std::string& target_service,
        const std::string& message_type,
        const mmo::common::RequestContext& context,
        const Request& request,
        ChannelCallOptions options = {}) {
        options.target_service = target_service;
        auto envelope =
            mmo::runtime::protocol::pack_message(message_type, context, request);
        return call_envelope(target_service, envelope, std::move(options));
    }

    std::size_t pending_count(const std::string& target_service) const;
    std::size_t pending_count_total() const;

private:
    ChannelConnectionPool connection_pool_;
};

}  // namespace mmo::runtime::channel

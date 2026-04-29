#include <vector>

#include "internal/gateway_player.pb.h"
#include "internal/instance_player.pb.h"
#include "modules/player/player_service.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_server.h"

namespace {

std::vector<mmo::modules::player::Reward> to_rewards(
    const google::protobuf::RepeatedPtrField<mmo::common::Reward>& rewards) {
    std::vector<mmo::modules::player::Reward> result;
    for (const auto& reward : rewards) {
        result.push_back(
            mmo::modules::player::Reward{reward.type(), reward.amount()});
    }
    return result;
}

}  // namespace

int main() {
    mmo::runtime::foundation::ServerApp app("player_server");
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(
            app.config().transport.tcp, app.config().execution);

    mmo::modules::player::PlayerService service;
    mmo::runtime::rpc::RpcServer rpc_server(
        mmo::runtime::rpc::make_rpc_server_options(app.config()));

    rpc_server.on(
        mmo::runtime::protocol::kGrantInstanceRewardRequest,
        [&service](const mmo::common::Envelope& envelope) {
            mmo::internal_api::GrantInstanceRewardRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid grant instance reward request");
            }

            const auto applied = service.apply_reward(
                request.context().player_id(),
                request.reward_grant_id(),
                to_rewards(request.rewards()));

            mmo::internal_api::GrantInstanceRewardResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_applied(applied.applied);
            response.set_gold(applied.gold);
            response.set_exp(applied.exp);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGrantInstanceRewardResponse,
                request.context(),
                response);
        });

    rpc_server.on(
        mmo::runtime::protocol::kGatewayApplyRewardRequest,
        [&service](const mmo::common::Envelope& envelope) {
            mmo::internal_api::GatewayApplyRewardRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid apply reward request");
            }

            const auto applied = service.apply_reward(
                request.context().player_id(),
                request.idempotency_key(),
                to_rewards(request.rewards()));

            mmo::internal_api::GatewayApplyRewardResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_applied(applied.applied);
            response.set_gold(applied.gold);
            response.set_exp(applied.exp);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGatewayApplyRewardResponse,
                request.context(),
                response);
        });

    mmo::runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        rpc_server.handler(),
        app.service_name(),
        tcp_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

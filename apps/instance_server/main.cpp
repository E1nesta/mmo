#include "internal/instance_player.pb.h"
#include "modules/instance/instance_service.h"
#include "public/instance.pb.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_router.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_client.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::runtime::foundation::ServerApp app("instance_server");
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(app.config().transport.tcp);
    mmo::runtime::transport::TcpEnvelopeClient upstream_client(tcp_options);

    mmo::modules::instance::InstanceService service;
    mmo::runtime::protocol::MessageRouter router;

    router.on(
        mmo::runtime::protocol::kEnterInstanceRequest,
        [&service](const mmo::common::Envelope& envelope) {
            mmo::public_api::EnterInstanceRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid enter instance request");
            }

            const auto instance = service.enter_instance(
                request.context().player_id(),
                request.dungeon_id());

            mmo::public_api::EnterInstanceResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_instance_id(instance.instance_id);
            response.set_boss_entity_id(instance.boss_entity_id);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kEnterInstanceResponse,
                request.context(),
                response);
        });

    router.on(
        mmo::runtime::protocol::kSettleInstanceRequest,
        [&service, &app, &upstream_client](const mmo::common::Envelope& envelope) {
            mmo::public_api::SettleInstanceRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid settle instance request");
            }

            const auto settled = service.settle_instance(
                request.context().player_id(),
                request.instance_id(),
                request.idempotency_key(),
                request.win());

            mmo::internal_api::GrantInstanceRewardRequest reward_request;
            *reward_request.mutable_context() = request.context();
            reward_request.set_reward_grant_id(settled.reward_grant_id);
            for (const auto& reward : settled.rewards) {
                auto* proto_reward = reward_request.add_rewards();
                proto_reward->set_type(reward.type);
                proto_reward->set_amount(reward.amount);
            }

            const auto reward_envelope = mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGrantInstanceRewardRequest,
                request.context(),
                reward_request);
            const auto reward_response_envelope = upstream_client.send(
                mmo::runtime::transport::make_transport_endpoint(
                    app.service_config("player_server")),
                reward_envelope);

            mmo::internal_api::GrantInstanceRewardResponse reward_response;
            if (!mmo::runtime::protocol::unpack_message(
                    reward_response_envelope, reward_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, "invalid player reward response");
            }
            if (!reward_response.context().success()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    reward_response.context().error_code(),
                    reward_response.context().error_message());
            }

            mmo::public_api::SettleInstanceResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_reward_grant_id(settled.reward_grant_id);
            response.set_duplicate(settled.duplicate);
            for (const auto& reward : settled.rewards) {
                auto* proto_reward = response.add_rewards();
                proto_reward->set_type(reward.type);
                proto_reward->set_amount(reward.amount);
            }

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kSettleInstanceResponse,
                request.context(),
                response);
        });

    mmo::runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        router.handler(),
        app.service_name(),
        tcp_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

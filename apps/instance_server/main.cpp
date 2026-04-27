#include <iostream>

#include "modules/instance/instance_service.h"
#include "public/instance.pb.h"
#include "public/player.pb.h"
#include "runtime/foundation/service_ports.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::modules::instance::InstanceService service;

    mmo::runtime::transport::TcpEnvelopeServer server(
        mmo::runtime::foundation::kInstanceServerPort,
        [&service](const mmo::public_api::Envelope& envelope) {
            if (envelope.message_type() == mmo::runtime::protocol::kEnterInstanceRequest) {
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
            }

            if (envelope.message_type() == mmo::runtime::protocol::kSettleInstanceRequest) {
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

                mmo::public_api::ApplyRewardRequest reward_request;
                *reward_request.mutable_context() = request.context();
                reward_request.set_idempotency_key(settled.reward_grant_id);
                for (const auto& reward : settled.rewards) {
                    auto* proto_reward = reward_request.add_rewards();
                    proto_reward->set_type(reward.type);
                    proto_reward->set_amount(reward.amount);
                }

                const auto reward_envelope = mmo::runtime::protocol::pack_message(
                    mmo::runtime::protocol::kApplyRewardRequest,
                    request.context(),
                    reward_request);
                mmo::runtime::transport::send_envelope(
                    mmo::runtime::foundation::kLocalhost,
                    mmo::runtime::foundation::kPlayerServerPort,
                    reward_envelope);

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
            }

            return mmo::runtime::protocol::make_error_envelope(
                envelope, 404, "unsupported instance message");
        });

    std::cout << "instance_server starting\n";
    return server.run();
}

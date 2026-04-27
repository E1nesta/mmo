#include <iostream>
#include <vector>

#include "modules/player/player_service.h"
#include "public/player.pb.h"
#include "runtime/foundation/service_ports.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"
#include "runtime/transport/tcp_envelope_server.h"

int main() {
    mmo::modules::player::PlayerService service;

    mmo::runtime::transport::TcpEnvelopeServer server(
        mmo::runtime::foundation::kPlayerServerPort,
        [&service](const mmo::public_api::Envelope& envelope) {
            if (envelope.message_type() != mmo::runtime::protocol::kApplyRewardRequest) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "unsupported player message");
            }

            mmo::public_api::ApplyRewardRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid apply reward request");
            }

            std::vector<mmo::modules::player::Reward> rewards;
            for (const auto& reward : request.rewards()) {
                rewards.push_back(
                    mmo::modules::player::Reward{reward.type(), reward.amount()});
            }

            const auto applied = service.apply_reward(
                request.context().player_id(),
                request.idempotency_key(),
                rewards);

            mmo::public_api::ApplyRewardResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_applied(applied.applied);
            response.set_gold(applied.gold);
            response.set_exp(applied.exp);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kApplyRewardResponse,
                request.context(),
                response);
        });

    std::cout << "player_server starting\n";
    return server.run();
}

#include "apps/game_gateway_server/gateway_proxy_handlers.h"

#include <string>

#include "apps/game_gateway_server/gateway_session_handlers.h"
#include "internal/gateway_world.pb.h"
#include "internal/gateway_auth.pb.h"
#include "internal/gateway_instance.pb.h"
#include "internal/gateway_player.pb.h"
#include "internal/gateway_social.pb.h"
#include "public/auth.pb.h"
#include "public/instance.pb.h"
#include "public/player.pb.h"
#include "public/social.pb.h"
#include "public/world.pb.h"
#include "runtime/channel/channel_call_options.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/message_types.h"

namespace mmo::apps::game_gateway_server {
namespace {

mmo::runtime::channel::ChannelCallOptions make_forward_options(
    const mmo::runtime::routing::RouteTarget& route,
    const mmo::common::RequestContext& context,
    const std::string& route_key = {}) {
    mmo::runtime::channel::ChannelCallOptions options;
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

}  // namespace

void register_gateway_proxy_handlers(
    mmo::runtime::routing::GatewayRouter& gateway_router,
    mmo::runtime::routing::GatewayForwarder& forwarder,
    const mmo::runtime::routing::RouteTable& route_table,
    mmo::runtime::session::SessionRegistry& sessions,
    mmo::runtime::session::RedisSessionStore& redis_sessions,
    mmo::runtime::observability::MetricsRegistry& security_metrics) {
    gateway_router.on(
        mmo::runtime::protocol::kLoginRequest,
        [&forwarder, &route_table](const mmo::common::Envelope& envelope) {
            mmo::public_api::LoginRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid login request");
            }

            const auto route = route_table.find(envelope.message_type());
            if (!route.has_value()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "gateway route is not configured");
            }

            mmo::internal_api::GatewayAuthLoginRequest internal_request;
            *internal_request.mutable_context() = request.context();
            internal_request.set_account_name(request.account_name());
            internal_request.set_password(request.password());
            internal_request.set_device_id(request.device_id());

            const auto forward_result = forwarder.forward(
                route->target_service,
                route->target_message_type,
                request.context(),
                internal_request,
                make_forward_options(*route, request.context()));
            if (!forward_result.ok()) {
                return forward_result.make_error_envelope(envelope);
            }

            mmo::internal_api::GatewayAuthLoginResponse internal_response;
            if (!mmo::runtime::protocol::unpack_message(
                    forward_result.response(), internal_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, "invalid auth response");
            }
            if (!internal_response.context().success()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    internal_response.context().error_code(),
                    internal_response.context().error_message());
            }

            mmo::public_api::LoginResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_account_id(internal_response.account_id());
            response.set_player_id(internal_response.player_id());
            response.set_session_token(internal_response.session_token());
            response.set_expires_at_epoch_seconds(
                internal_response.expires_at_epoch_seconds());
            response.set_access_token(internal_response.access_token());
            response.set_gateway_ticket(internal_response.gateway_ticket());
            response.set_access_token_expires_at_epoch_millis(
                internal_response.access_token_expires_at_epoch_millis());
            response.set_gateway_ticket_expires_at_epoch_millis(
                internal_response.gateway_ticket_expires_at_epoch_millis());

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kLoginResponse,
                request.context(),
                response);
        });

    gateway_router.on(
        mmo::runtime::protocol::kEnterInstanceRequest,
        [&sessions, &redis_sessions, &security_metrics, &forwarder, &route_table](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::EnterInstanceRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid enter instance request");
            }
            const auto validation_error =
                validate_bound_public_request(
                    envelope,
                    sessions,
                    redis_sessions,
                    request.context(),
                    &security_metrics);
            if (validation_error.has_value()) {
                return *validation_error;
            }

            const auto route = route_table.find(envelope.message_type());
            if (!route.has_value()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "gateway route is not configured");
            }

            mmo::internal_api::GatewayEnterInstanceRequest internal_request;
            *internal_request.mutable_context() = request.context();
            internal_request.set_dungeon_id(request.dungeon_id());

            const auto forward_result = forwarder.forward(
                route->target_service,
                route->target_message_type,
                request.context(),
                internal_request,
                make_forward_options(*route, request.context()));
            if (!forward_result.ok()) {
                return forward_result.make_error_envelope(envelope);
            }

            mmo::internal_api::GatewayEnterInstanceResponse internal_response;
            if (!mmo::runtime::protocol::unpack_message(
                    forward_result.response(), internal_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, "invalid instance response");
            }
            if (!internal_response.context().success()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    internal_response.context().error_code(),
                    internal_response.context().error_message());
            }

            mmo::public_api::EnterInstanceResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_instance_id(internal_response.instance_id());
            response.set_boss_entity_id(internal_response.boss_entity_id());

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kEnterInstanceResponse,
                request.context(),
                response);
        });

    gateway_router.on(
        mmo::runtime::protocol::kSettleInstanceRequest,
        [&sessions, &redis_sessions, &security_metrics, &forwarder, &route_table](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::SettleInstanceRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid settle instance request");
            }
            const auto validation_error =
                validate_bound_public_request(
                    envelope,
                    sessions,
                    redis_sessions,
                    request.context(),
                    &security_metrics);
            if (validation_error.has_value()) {
                return *validation_error;
            }

            const auto route = route_table.find(envelope.message_type());
            if (!route.has_value()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "gateway route is not configured");
            }

            mmo::internal_api::GatewaySettleInstanceRequest internal_request;
            *internal_request.mutable_context() = request.context();
            internal_request.set_instance_id(request.instance_id());
            internal_request.set_idempotency_key(request.idempotency_key());
            internal_request.set_win(request.win());

            const auto forward_result = forwarder.forward(
                route->target_service,
                route->target_message_type,
                request.context(),
                internal_request,
                make_forward_options(
                    *route,
                    request.context(),
                    std::to_string(request.instance_id())));
            if (!forward_result.ok()) {
                return forward_result.make_error_envelope(envelope);
            }

            mmo::internal_api::GatewaySettleInstanceResponse internal_response;
            if (!mmo::runtime::protocol::unpack_message(
                    forward_result.response(), internal_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, "invalid instance response");
            }
            if (!internal_response.context().success()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    internal_response.context().error_code(),
                    internal_response.context().error_message());
            }

            mmo::public_api::SettleInstanceResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_reward_grant_id(internal_response.reward_grant_id());
            response.set_duplicate(internal_response.duplicate());
            for (const auto& reward : internal_response.rewards()) {
                *response.add_rewards() = reward;
            }

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kSettleInstanceResponse,
                request.context(),
                response);
        });

    gateway_router.on(
        mmo::runtime::protocol::kApplyRewardRequest,
        [&sessions, &redis_sessions, &security_metrics, &forwarder, &route_table](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::ApplyRewardRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid apply reward request");
            }
            const auto validation_error =
                validate_bound_public_request(
                    envelope,
                    sessions,
                    redis_sessions,
                    request.context(),
                    &security_metrics);
            if (validation_error.has_value()) {
                return *validation_error;
            }

            const auto route = route_table.find(envelope.message_type());
            if (!route.has_value()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "gateway route is not configured");
            }

            mmo::internal_api::GatewayApplyRewardRequest internal_request;
            *internal_request.mutable_context() = request.context();
            internal_request.set_idempotency_key(request.idempotency_key());
            for (const auto& reward : request.rewards()) {
                *internal_request.add_rewards() = reward;
            }

            const auto forward_result = forwarder.forward(
                route->target_service,
                route->target_message_type,
                request.context(),
                internal_request,
                make_forward_options(*route, request.context()));
            if (!forward_result.ok()) {
                return forward_result.make_error_envelope(envelope);
            }

            mmo::internal_api::GatewayApplyRewardResponse internal_response;
            if (!mmo::runtime::protocol::unpack_message(
                    forward_result.response(), internal_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, "invalid player response");
            }
            if (!internal_response.context().success()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    internal_response.context().error_code(),
                    internal_response.context().error_message());
            }

            mmo::public_api::ApplyRewardResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_applied(internal_response.applied());
            response.set_gold(internal_response.gold());
            response.set_exp(internal_response.exp());

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kApplyRewardResponse,
                request.context(),
                response);
        });

    gateway_router.on(
        mmo::runtime::protocol::kSocialBoundaryRequest,
        [&sessions, &redis_sessions, &security_metrics, &forwarder, &route_table](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::SocialBoundaryRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid social boundary request");
            }
            const auto validation_error =
                validate_bound_public_request(
                    envelope,
                    sessions,
                    redis_sessions,
                    request.context(),
                    &security_metrics);
            if (validation_error.has_value()) {
                return *validation_error;
            }

            const auto route = route_table.find(envelope.message_type());
            if (!route.has_value()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "gateway route is not configured");
            }

            mmo::internal_api::GatewaySocialBoundaryRequest internal_request;
            *internal_request.mutable_context() = request.context();
            internal_request.set_target_player_id(request.target_player_id());

            const auto forward_result = forwarder.forward(
                route->target_service,
                route->target_message_type,
                request.context(),
                internal_request,
                make_forward_options(*route, request.context()));
            if (!forward_result.ok()) {
                return forward_result.make_error_envelope(envelope);
            }

            mmo::internal_api::GatewaySocialBoundaryResponse internal_response;
            if (!mmo::runtime::protocol::unpack_message(
                    forward_result.response(), internal_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, "invalid social response");
            }
            if (!internal_response.context().success()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    internal_response.context().error_code(),
                    internal_response.context().error_message());
            }

            mmo::public_api::SocialBoundaryResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_friend_boundary_available(
                internal_response.friend_boundary_available());
            response.set_chat_boundary_available(
                internal_response.chat_boundary_available());
            response.set_team_boundary_available(
                internal_response.team_boundary_available());

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kSocialBoundaryResponse,
                request.context(),
                response);
        });

    gateway_router.on(
        mmo::runtime::protocol::kEnterWorldRequest,
        [&sessions, &redis_sessions, &security_metrics, &forwarder, &route_table](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::EnterWorldRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid enter world request");
            }

            const auto route = route_table.find(envelope.message_type());
            if (!route.has_value()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 404, "gateway route is not configured");
            }
            if (route->require_session) {
                const auto validation_error =
                    validate_bound_public_request(
                        envelope,
                        sessions,
                        redis_sessions,
                        request.context(),
                        &security_metrics);
                if (validation_error.has_value()) {
                    return *validation_error;
                }
            }

            mmo::internal_api::GatewayEnterWorldRequest internal_request;
            *internal_request.mutable_context() = request.context();
            internal_request.set_preferred_map_id(request.preferred_map_id());
            internal_request.set_preferred_line_id(request.preferred_line_id());

            const auto forward_result = forwarder.forward(
                route->target_service,
                route->target_message_type,
                request.context(),
                internal_request,
                make_forward_options(*route, request.context()));
            if (!forward_result.ok()) {
                return forward_result.make_error_envelope(envelope);
            }

            mmo::internal_api::GatewayEnterWorldResponse internal_response;
            if (!mmo::runtime::protocol::unpack_message(
                    forward_result.response(), internal_response)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 502, "invalid world response");
            }
            if (!internal_response.context().success()) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope,
                    internal_response.context().error_code(),
                    internal_response.context().error_message());
            }

            mmo::public_api::EnterWorldResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            *response.mutable_route() = internal_response.route();
            response.set_scene_entity_id(internal_response.scene_entity_id());
            *response.mutable_spawn_position() = internal_response.spawn_position();

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kEnterWorldResponse,
                request.context(),
                response);
        });
}

}  // namespace mmo::apps::game_gateway_server

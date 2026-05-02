#include <memory>
#include <optional>
#include <string>

#include "internal/gateway_world.pb.h"
#include "internal/gateway_auth.pb.h"
#include "internal/gateway_instance.pb.h"
#include "internal/gateway_player.pb.h"
#include "internal/gateway_social.pb.h"
#include "public/auth.pb.h"
#include "public/gateway.pb.h"
#include "public/instance.pb.h"
#include "public/player.pb.h"
#include "public/social.pb.h"
#include "public/world.pb.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/observability/metrics.h"
#include "runtime/protocol/auth_tokens.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/internal_auth.h"
#include "runtime/protocol/message_types.h"
#include "runtime/routing/gateway_forwarder.h"
#include "runtime/routing/gateway_router.h"
#include "runtime/routing/route_table.h"
#include "runtime/session/redis_session_store.h"
#include "runtime/session/redis_ticket_replay_store.h"
#include "runtime/session/session_context.h"
#include "runtime/storage/storage_bootstrap.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_server.h"

namespace {

std::optional<mmo::common::Envelope> validate_bound_public_request(
    const mmo::common::Envelope& envelope,
    const mmo::runtime::session::SessionRegistry& sessions,
    const mmo::runtime::session::RedisSessionStore& redis_sessions,
    const mmo::common::RequestContext& context,
    mmo::runtime::observability::MetricsRegistry* metrics = nullptr) {
    if (envelope.player_id() != context.player_id() ||
        envelope.session_token() != context.session_token() ||
        envelope.game_session_id() != context.game_session_id()) {
        return mmo::runtime::protocol::make_error_envelope(
            envelope, 400, "request context does not match envelope");
    }
    const auto now_millis = static_cast<std::uint64_t>(
        mmo::runtime::protocol::current_time_millis());
    std::string redis_error;
    if (envelope.game_session_id().empty() ||
        !sessions.is_bound(
            envelope.player_id(),
            envelope.session_token(),
            envelope.game_session_id(),
            now_millis) ||
        !redis_sessions.is_bound(
            envelope.player_id(),
            envelope.session_token(),
            envelope.game_session_id(),
            now_millis,
            &redis_error)) {
        if (metrics != nullptr) {
            metrics->record_game_session_expired();
        }
        return mmo::runtime::protocol::make_error_envelope(
            envelope, 401, "game session is not bound to gateway");
    }
    return std::nullopt;
}

mmo::runtime::protocol::AuthTokenOptions make_token_options(
    const mmo::runtime::foundation::GatewayTicketConfig& config) {
    mmo::runtime::protocol::AuthTokenOptions options;
    options.issuer = config.issuer;
    options.access_audience = config.access_audience;
    options.gateway_audience = config.gateway_audience;
    options.active_key_id = config.active_key_id;
    options.active_shared_secret = config.active_shared_secret;
    options.previous_key_id = config.previous_key_id;
    options.previous_shared_secret = config.previous_shared_secret;
    options.previous_key_accept_millis = config.previous_key_accept_millis;
    return options;
}

bool issue_reconnect_ticket(
    const mmo::runtime::foundation::ServerConfig& config,
    mmo::runtime::session::RedisSessionStore& redis_sessions,
    const mmo::runtime::session::ConnectionBinding& binding,
    std::uint64_t now_millis,
    std::string* reconnect_ticket,
    std::int64_t* reconnect_ticket_expires_at,
    std::string* error_message) {
    if (reconnect_ticket == nullptr || reconnect_ticket_expires_at == nullptr) {
        if (error_message != nullptr) {
            *error_message = "reconnect ticket output is null";
        }
        return false;
    }
    if (!mmo::runtime::protocol::issue_auth_token(
            mmo::runtime::protocol::AuthTokenPurpose::kReconnect,
            binding.account_id,
            binding.player_id,
            binding.session_token,
            static_cast<std::int64_t>(now_millis),
            config.security.gateway_session.reconnect_ticket_ttl_millis,
            make_token_options(config.security.gateway_ticket),
            reconnect_ticket,
            reconnect_ticket_expires_at,
            error_message)) {
        return false;
    }

    mmo::runtime::protocol::AuthTokenClaims claims;
    if (!mmo::runtime::protocol::validate_auth_token(
            *reconnect_ticket,
            mmo::runtime::protocol::AuthTokenPurpose::kReconnect,
            config.security.gateway_ticket.gateway_audience,
            make_token_options(config.security.gateway_ticket),
            static_cast<std::int64_t>(now_millis),
            &claims,
            error_message)) {
        return false;
    }

    mmo::runtime::session::ReconnectTicket redis_ticket;
    redis_ticket.account_id = binding.account_id;
    redis_ticket.connection_id = binding.connection_id;
    redis_ticket.game_session_id = binding.game_session_id;
    redis_ticket.player_id = binding.player_id;
    redis_ticket.session_token = binding.session_token;
    redis_ticket.gateway_id = binding.gateway_id;
    redis_ticket.device_id = binding.device_id;
    redis_ticket.expire_at_millis =
        static_cast<std::uint64_t>(*reconnect_ticket_expires_at);
    return redis_sessions.save_reconnect_ticket(
        claims.jti, redis_ticket, now_millis, error_message);
}

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

int main() {
    mmo::runtime::foundation::ServerApp app("game_gateway_server");
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(
            app.config().transport.tcp, app.config().execution);

    std::shared_ptr<mmo::runtime::storage::RedisConnectionPool> redis_pool;
    std::string storage_error;
    if (!mmo::runtime::storage::initialize_redis_pool(
            app.config(), &redis_pool, &storage_error)) {
        mmo::runtime::observability::log_error(
            mmo::runtime::observability::LogContext{app.service_name()},
            "redis_pool_init_failed error=" + storage_error);
        return 1;
    }

    mmo::runtime::routing::GatewayForwarder forwarder(
        app.service_name(),
        app.config(),
        tcp_options);
    mmo::runtime::routing::RouteTable route_table;
    route_table.add(
        mmo::runtime::protocol::kLoginRequest,
        mmo::runtime::routing::RouteTarget{
            "auth_server",
            mmo::runtime::protocol::kGatewayAuthLoginRequest,
            false,
            mmo::runtime::channel::RoutingPolicy::kLeastPending,
            "",
            ""});
    route_table.add(
        mmo::runtime::protocol::kEnterWorldRequest,
        mmo::runtime::routing::RouteTarget{
            "world_server",
            mmo::runtime::protocol::kGatewayEnterWorldRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::runtime::protocol::kEnterInstanceRequest,
        mmo::runtime::routing::RouteTarget{
            "instance_server",
            mmo::runtime::protocol::kGatewayEnterInstanceRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::runtime::protocol::kSettleInstanceRequest,
        mmo::runtime::routing::RouteTarget{
            "instance_server",
            mmo::runtime::protocol::kGatewaySettleInstanceRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyInstance,
            "",
            ""});
    route_table.add(
        mmo::runtime::protocol::kApplyRewardRequest,
        mmo::runtime::routing::RouteTarget{
            "player_server",
            mmo::runtime::protocol::kGatewayApplyRewardRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});
    route_table.add(
        mmo::runtime::protocol::kSocialBoundaryRequest,
        mmo::runtime::routing::RouteTarget{
            "social_server",
            mmo::runtime::protocol::kGatewaySocialBoundaryRequest,
            true,
            mmo::runtime::channel::RoutingPolicy::kStickyPlayer,
            "player_id",
            ""});

    mmo::runtime::session::SessionRegistry sessions;
    mmo::runtime::session::RedisTicketReplayStore ticket_replay_guard(redis_pool);
    mmo::runtime::session::RedisSessionStore redis_sessions(redis_pool);
    mmo::runtime::observability::MetricsRegistry security_metrics;
    mmo::runtime::routing::GatewayRouter gateway_router;

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
        mmo::runtime::protocol::kGateLoginRequest,
        [&sessions, &redis_sessions, &ticket_replay_guard, &security_metrics, &app](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::GateLoginRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid gate login request");
            }
            if (envelope.player_id() != request.player_id() ||
                envelope.session_token() != request.session_token() ||
                request.context().player_id() != request.player_id() ||
                request.context().session_token() != request.session_token()) {
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "gate login context does not match envelope");
            }

            mmo::runtime::protocol::AuthTokenClaims claims;
            std::string token_error;
            if (!mmo::runtime::protocol::validate_auth_token(
                    request.gateway_ticket(),
                    mmo::runtime::protocol::AuthTokenPurpose::kGateway,
                    app.config().security.gateway_ticket.gateway_audience,
                    make_token_options(app.config().security.gateway_ticket),
                    mmo::runtime::protocol::current_time_millis(),
                    &claims,
                    &token_error)) {
                security_metrics.record_gateway_ticket_rejected();
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "gateway ticket authentication failed");
            }
            if (claims.player_id != request.player_id() ||
                claims.session_token != request.session_token()) {
                security_metrics.record_gateway_ticket_rejected();
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "gateway ticket does not match request");
            }
            const auto now_millis = static_cast<std::uint64_t>(
                mmo::runtime::protocol::current_time_millis());
            if (!ticket_replay_guard.consume(
                    claims.jti,
                    now_millis,
                    static_cast<std::uint64_t>(
                        claims.expires_at_epoch_millis))) {
                security_metrics.record_gateway_ticket_replay();
                security_metrics.record_gateway_ticket_rejected();
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "gateway ticket authentication failed");
            }

            const auto expires_at =
                now_millis +
                static_cast<std::uint64_t>(
                    app.config().security.gateway_session.game_session_ttl_millis);
            const auto binding = sessions.bind(
                claims.account_id,
                request.player_id(),
                request.session_token(),
                app.config().security.gateway_session.gateway_id,
                request.device_id(),
                now_millis,
                expires_at,
                static_cast<std::uint64_t>(
                    app.config()
                        .security
                        .gateway_session
                        .heartbeat_timeout_millis));
            std::string session_store_error;
            if (!redis_sessions.save_binding(binding, &session_store_error)) {
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 503, "gateway session store is unavailable");
            }
            std::string reconnect_ticket;
            std::int64_t reconnect_ticket_expires_at = 0;
            if (!issue_reconnect_ticket(
                    app.config(),
                    redis_sessions,
                    binding,
                    now_millis,
                    &reconnect_ticket,
                    &reconnect_ticket_expires_at,
                    &session_store_error)) {
                security_metrics.record_gate_login_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 503, "reconnect ticket issue failed");
            }
            security_metrics.record_gate_login_success();

            mmo::public_api::GateLoginResponse response;
            auto response_context = request.context();
            response_context.set_game_session_id(binding.game_session_id);
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(response_context);
            response.set_bound(true);
            response.set_connection_id(binding.connection_id);
            response.set_game_session_id(binding.game_session_id);
            response.set_expires_at_epoch_millis(
                static_cast<std::int64_t>(binding.expire_at_millis));
            response.set_reconnect_ticket(reconnect_ticket);
            response.set_reconnect_ticket_expires_at_epoch_millis(
                reconnect_ticket_expires_at);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGateLoginResponse,
                response_context,
                response);
        });

    gateway_router.on(
        mmo::runtime::protocol::kReconnectRequest,
        [&sessions, &redis_sessions, &security_metrics, &app](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::ReconnectRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid reconnect request");
            }
            if (envelope.player_id() != request.player_id() ||
                envelope.session_token() != request.session_token() ||
                envelope.game_session_id() != request.game_session_id() ||
                request.context().player_id() != request.player_id() ||
                request.context().session_token() != request.session_token() ||
                request.context().game_session_id() != request.game_session_id()) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "reconnect context does not match envelope");
            }

            const auto now_millis = static_cast<std::uint64_t>(
                mmo::runtime::protocol::current_time_millis());
            mmo::runtime::protocol::AuthTokenClaims claims;
            std::string token_error;
            if (!mmo::runtime::protocol::validate_auth_token(
                    request.reconnect_ticket(),
                    mmo::runtime::protocol::AuthTokenPurpose::kReconnect,
                    app.config().security.gateway_ticket.gateway_audience,
                    make_token_options(app.config().security.gateway_ticket),
                    static_cast<std::int64_t>(now_millis),
                    &claims,
                    &token_error)) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "reconnect ticket authentication failed");
            }
            if (claims.player_id != request.player_id() ||
                claims.session_token != request.session_token()) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "reconnect ticket does not match request");
            }

            mmo::runtime::session::ReconnectTicket consumed_ticket;
            std::string session_store_error;
            if (!redis_sessions.consume_reconnect_ticket(
                    claims.jti,
                    now_millis,
                    &consumed_ticket,
                    &session_store_error) ||
                consumed_ticket.account_id != claims.account_id ||
                consumed_ticket.player_id != request.player_id() ||
                consumed_ticket.session_token != request.session_token() ||
                consumed_ticket.game_session_id != request.game_session_id() ||
                consumed_ticket.gateway_id !=
                    app.config().security.gateway_session.gateway_id) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "reconnect ticket authentication failed");
            }

            const auto expires_at =
                now_millis +
                static_cast<std::uint64_t>(
                    app.config().security.gateway_session.game_session_ttl_millis);
            const auto binding = sessions.reconnect(
                consumed_ticket.account_id,
                request.player_id(),
                request.session_token(),
                request.game_session_id(),
                app.config().security.gateway_session.gateway_id,
                request.device_id().empty()
                    ? consumed_ticket.device_id
                    : request.device_id(),
                now_millis,
                expires_at,
                static_cast<std::uint64_t>(
                    app.config()
                        .security
                        .gateway_session
                        .heartbeat_timeout_millis));
            if (!redis_sessions.save_binding(binding, &session_store_error)) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 503, "gateway session store is unavailable");
            }

            std::string next_reconnect_ticket;
            std::int64_t next_reconnect_ticket_expires_at = 0;
            if (!issue_reconnect_ticket(
                    app.config(),
                    redis_sessions,
                    binding,
                    now_millis,
                    &next_reconnect_ticket,
                    &next_reconnect_ticket_expires_at,
                    &session_store_error)) {
                security_metrics.record_reconnect_failed();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 503, "reconnect ticket issue failed");
            }
            security_metrics.record_reconnect_success();

            mmo::public_api::ReconnectResponse response;
            auto response_context = request.context();
            response_context.set_game_session_id(binding.game_session_id);
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(response_context);
            response.set_reconnected(true);
            response.set_connection_id(binding.connection_id);
            response.set_game_session_id(binding.game_session_id);
            response.set_expires_at_epoch_millis(
                static_cast<std::int64_t>(binding.expire_at_millis));
            response.set_reconnect_ticket(next_reconnect_ticket);
            response.set_reconnect_ticket_expires_at_epoch_millis(
                next_reconnect_ticket_expires_at);
            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kReconnectResponse,
                response_context,
                response);
        });

    gateway_router.on(
        mmo::runtime::protocol::kPingRequest,
        [&sessions, &redis_sessions, &security_metrics, &app](
            const mmo::common::Envelope& envelope) {
            mmo::public_api::PingRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid ping request");
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
            const auto now_millis = static_cast<std::uint64_t>(
                mmo::runtime::protocol::current_time_millis());
            if (!sessions.touch(
                    envelope.player_id(), envelope.game_session_id(), now_millis)) {
                security_metrics.record_game_session_expired();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "game session is not bound to gateway");
            }
            std::string session_store_error;
            if (!redis_sessions.touch_binding(
                    envelope.player_id(),
                    envelope.game_session_id(),
                    now_millis,
                    &session_store_error)) {
                security_metrics.record_game_session_expired();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "game session is not bound to gateway");
            }
            const auto binding = sessions.find(envelope.player_id());
            if (!binding.has_value()) {
                security_metrics.record_game_session_expired();
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 401, "game session is not bound to gateway");
            }
            std::string reconnect_ticket;
            std::int64_t reconnect_ticket_expires_at = 0;
            if (!issue_reconnect_ticket(
                    app.config(),
                    redis_sessions,
                    *binding,
                    now_millis,
                    &reconnect_ticket,
                    &reconnect_ticket_expires_at,
                    &session_store_error)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 503, "reconnect ticket issue failed");
            }

            mmo::public_api::PingResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_server_time_ms(static_cast<std::int64_t>(now_millis));
            response.set_reconnect_ticket(reconnect_ticket);
            response.set_reconnect_ticket_expires_at_epoch_millis(
                reconnect_ticket_expires_at);
            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kPingResponse,
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

    mmo::runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        gateway_router.handler(),
        app.service_name(),
        tcp_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

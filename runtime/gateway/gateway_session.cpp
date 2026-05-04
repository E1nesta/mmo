#include "runtime/gateway/gateway_session.h"

namespace runtime::gateway {

runtime::protocol::AuthTokenOptions make_gateway_token_options(
    const runtime::foundation::GatewayTicketConfig& config) {
    runtime::protocol::AuthTokenOptions options;
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
    const runtime::foundation::ServerConfig& config,
    runtime::session::SessionStore& session_store,
    const runtime::session::ConnectionBinding& binding,
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
    if (!runtime::protocol::issue_auth_token(
            runtime::protocol::AuthTokenPurpose::kReconnect,
            binding.account_id,
            binding.player_id,
            binding.session_token,
            static_cast<std::int64_t>(now_millis),
            config.security.gateway_session.reconnect_ticket_ttl_millis,
            make_gateway_token_options(config.security.gateway_ticket),
            reconnect_ticket,
            reconnect_ticket_expires_at,
            error_message)) {
        return false;
    }

    runtime::protocol::AuthTokenClaims claims;
    if (!runtime::protocol::validate_auth_token(
            *reconnect_ticket,
            runtime::protocol::AuthTokenPurpose::kReconnect,
            config.security.gateway_ticket.gateway_audience,
            make_gateway_token_options(config.security.gateway_ticket),
            static_cast<std::int64_t>(now_millis),
            &claims,
            error_message)) {
        return false;
    }

    runtime::session::ReconnectTicket redis_ticket;
    redis_ticket.account_id = binding.account_id;
    redis_ticket.connection_id = binding.connection_id;
    redis_ticket.game_session_id = binding.game_session_id;
    redis_ticket.player_id = binding.player_id;
    redis_ticket.session_token = binding.session_token;
    redis_ticket.gateway_id = binding.gateway_id;
    redis_ticket.device_id = binding.device_id;
    redis_ticket.expire_at_millis =
        static_cast<std::uint64_t>(*reconnect_ticket_expires_at);
    return session_store.save_reconnect_ticket(
        claims.jti, redis_ticket, now_millis, error_message);
}

}  // namespace runtime::gateway

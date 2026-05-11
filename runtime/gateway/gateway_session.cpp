#include "runtime/gateway/gateway_session.h"

namespace runtime::gateway {

bool issue_reconnect_ticket(
    const GatewaySessionOptions& options,
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
            options.reconnect_ticket_ttl_millis,
            options.token_options,
            reconnect_ticket,
            reconnect_ticket_expires_at,
            error_message)) {
        return false;
    }

    runtime::protocol::AuthTokenClaims claims;
    if (!runtime::protocol::validate_auth_token(
            *reconnect_ticket,
            runtime::protocol::AuthTokenPurpose::kReconnect,
            options.gateway_audience,
            options.token_options,
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

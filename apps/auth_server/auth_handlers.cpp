#include "apps/auth_server/auth_handlers.h"

#include <cstdint>
#include <string>

#include "internal/gateway_auth.pb.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/auth_tokens.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/internal_auth.h"
#include "runtime/protocol/message_types.h"

namespace mmo::apps::auth_server {
namespace {

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

}  // namespace

void register_auth_handlers(
    mmo::runtime::rpc::RpcServer& rpc_server,
    mmo::modules::auth::AuthService& service,
    const mmo::runtime::foundation::ServerConfig& config,
    const std::string& service_name) {
    rpc_server.on(
        mmo::runtime::protocol::kGatewayAuthLoginRequest,
        [&service, &config, service_name](const mmo::common::Envelope& envelope) {
            mmo::internal_api::GatewayAuthLoginRequest request;
            if (!mmo::runtime::protocol::unpack_message(envelope, request)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 400, "invalid login request");
            }

            const auto login =
                service.login(
                    request.account_name(), request.password(), request.device_id());
            if (!login.success) {
                auto log_context =
                    mmo::runtime::observability::context_from_envelope(
                        service_name, envelope);
                mmo::runtime::observability::log_warn(
                    log_context,
                    "login_rejected reason=" + login.internal_reason);
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, login.error_code, login.error_message);
            }
            const auto now_millis = mmo::runtime::protocol::current_time_millis();
            const auto& ticket_config = config.security.gateway_ticket;
            const auto token_options = make_token_options(ticket_config);
            std::string access_token;
            std::string gateway_ticket;
            std::int64_t access_expires_at = 0;
            std::int64_t gateway_ticket_expires_at = 0;
            std::string token_error;
            if (!mmo::runtime::protocol::issue_auth_token(
                    mmo::runtime::protocol::AuthTokenPurpose::kAccess,
                    login.account_id,
                    login.player_id,
                    login.session_token,
                    now_millis,
                    ticket_config.access_token_ttl_millis,
                    token_options,
                    &access_token,
                    &access_expires_at,
                    &token_error) ||
                !mmo::runtime::protocol::issue_auth_token(
                    mmo::runtime::protocol::AuthTokenPurpose::kGateway,
                    login.account_id,
                    login.player_id,
                    login.session_token,
                    now_millis,
                    ticket_config.gateway_ticket_ttl_millis,
                    token_options,
                    &gateway_ticket,
                    &gateway_ticket_expires_at,
                    &token_error)) {
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, 500, token_error);
            }

            mmo::internal_api::GatewayAuthLoginResponse response;
            *response.mutable_context() =
                mmo::runtime::protocol::make_ok_context(request.context());
            response.set_account_id(login.account_id);
            response.set_player_id(login.player_id);
            response.set_session_token(login.session_token);
            response.set_expires_at_epoch_seconds(login.expires_at_epoch_seconds);
            response.set_access_token(access_token);
            response.set_gateway_ticket(gateway_ticket);
            response.set_access_token_expires_at_epoch_millis(access_expires_at);
            response.set_gateway_ticket_expires_at_epoch_millis(
                gateway_ticket_expires_at);

            return mmo::runtime::protocol::pack_message(
                mmo::runtime::protocol::kGatewayAuthLoginResponse,
                request.context(),
                response);
        });
}

}  // namespace mmo::apps::auth_server

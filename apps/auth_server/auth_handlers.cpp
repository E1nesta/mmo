#include "apps/auth_server/auth_handlers.h"

#include <cstdint>
#include <string>
#include <utility>

#include "internal/gateway_auth.pb.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/auth_tokens.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/internal_auth.h"
#include "apps/protocol/message_types.h"
#include "runtime/server/handler_result.h"
#include "runtime/server/typed_handler.h"

namespace apps::auth_server {

namespace app_proto = apps::protocol;
namespace foundation = runtime::foundation;
namespace observability = runtime::observability;
namespace protocol = runtime::protocol;
namespace rpc = runtime::rpc;
namespace server = runtime::server;
namespace auth = modules::auth;
namespace {

protocol::AuthTokenOptions make_token_options(
    const foundation::GatewayTicketConfig& config) {
    protocol::AuthTokenOptions options;
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
    rpc::RpcServer& rpc_server,
    auth::AuthService& service,
    const foundation::ServerConfig& config,
    const std::string& service_name) {
    server::bind_typed_handler<
        mmo::internal_api::GatewayAuthLoginRequest,
        mmo::internal_api::GatewayAuthLoginResponse>(
        rpc_server,
        app_proto::kGatewayAuthLoginRequest,
        app_proto::kGatewayAuthLoginResponse,
        service_name,
        [&service, &config](
            const mmo::internal_api::GatewayAuthLoginRequest& request,
            const server::ServiceContext& context) {
            const auto login =
                service.login(
                    request.account_name(), request.password(), request.device_id());
            if (!login.success) {
                observability::LogContext log_context{
                    context.service_name};
                log_context.request_id = context.request.request_id();
                log_context.player_id = context.request.player_id();
                log_context.message_type = context.message_type;
                log_context.trace_id = context.request.trace_id();
                log_context.error_code = login.error_code;
                observability::log_warn(
                    log_context,
                    "login_rejected reason=" + login.internal_reason);
                return server::HandlerResult<
                    mmo::internal_api::GatewayAuthLoginResponse>::failure(
                        login.error_code, login.error_message);
            }
            const auto now_millis = protocol::current_time_millis();
            const auto& ticket_config = config.security.gateway_ticket;
            const auto token_options = make_token_options(ticket_config);
            std::string access_token;
            std::string gateway_ticket;
            std::int64_t access_expires_at = 0;
            std::int64_t gateway_ticket_expires_at = 0;
            std::string token_error;
            if (!protocol::issue_auth_token(
                    protocol::AuthTokenPurpose::kAccess,
                    login.account_id,
                    login.player_id,
                    login.session_token,
                    now_millis,
                    ticket_config.access_token_ttl_millis,
                    token_options,
                    &access_token,
                    &access_expires_at,
                    &token_error) ||
                !protocol::issue_auth_token(
                    protocol::AuthTokenPurpose::kGateway,
                    login.account_id,
                    login.player_id,
                    login.session_token,
                    now_millis,
                    ticket_config.gateway_ticket_ttl_millis,
                    token_options,
                    &gateway_ticket,
                    &gateway_ticket_expires_at,
                    &token_error)) {
                return server::HandlerResult<
                    mmo::internal_api::GatewayAuthLoginResponse>::failure(
                        500, token_error);
            }

            mmo::internal_api::GatewayAuthLoginResponse response;
            *response.mutable_context() =
                protocol::make_ok_context(request.context());
            response.set_account_id(login.account_id);
            response.set_player_id(login.player_id);
            response.set_session_token(login.session_token);
            response.set_expires_at_epoch_seconds(login.expires_at_epoch_seconds);
            response.set_access_token(access_token);
            response.set_gateway_ticket(gateway_ticket);
            response.set_access_token_expires_at_epoch_millis(access_expires_at);
            response.set_gateway_ticket_expires_at_epoch_millis(
                gateway_ticket_expires_at);

            return server::HandlerResult<
                mmo::internal_api::GatewayAuthLoginResponse>::success(
                    std::move(response));
        });
}

}  // namespace apps::auth_server

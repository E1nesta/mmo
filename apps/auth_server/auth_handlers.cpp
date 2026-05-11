#include "apps/auth_server/auth_handlers.h"

#include <cstdint>
#include <string>
#include <utility>

#include "ss/gateway_auth.pb.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/auth_tokens.h"
#include "proto/message_catalog.h"
#include "runtime/handler/handler_result.h"
#include "runtime/handler/typed_handler.h"
#include "runtime/protocol/payload_utils.h"

namespace apps::auth_server {

namespace app_proto = mmo::protocol;
namespace foundation = runtime::foundation;
namespace observability = runtime::observability;
namespace protocol = runtime::protocol;
namespace rpc = runtime::rpc;
namespace handler = runtime::handler;
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
    rpc::RpcDispatcher& dispatcher,
    auth::AuthService& service,
    const foundation::ServerConfig& config,
    const std::string& service_name) {
    handler::bind_typed_handler<
        mmo::ss::GatewayAuthLoginRequest,
        mmo::ss::GatewayAuthLoginResponse>(
        dispatcher,
        app_proto::kGatewayAuthLoginRequest,
        app_proto::kGatewayAuthLoginResponse,
        service_name,
        [&service, &config](
            const mmo::ss::GatewayAuthLoginRequest& request,
            const handler::HandlerContext& context) {
            const auto login =
                service.login(
                    request.account_name(), request.password(), request.device_id());
            if (!login.success) {
                observability::LogContext log_context{
                    context.service_name};
                log_context.request_id = context.request_id;
                log_context.route_key = context.route_key;
                log_context.message_id = context.message_id;
                log_context.error_code = login.error_code;
                observability::log_warn(
                    log_context,
                    "login_rejected reason=" + login.internal_reason);
                return handler::HandlerResult<
                    mmo::ss::GatewayAuthLoginResponse>::failure(
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
                return handler::HandlerResult<
                    mmo::ss::GatewayAuthLoginResponse>::failure(
                        500, token_error);
            }

            mmo::ss::GatewayAuthLoginResponse response;
            *response.mutable_result() = protocol::make_ok_result();
            response.set_account_id(login.account_id);
            response.set_player_id(login.player_id);
            response.set_session_token(login.session_token);
            response.set_expires_at_epoch_seconds(login.expires_at_epoch_seconds);
            response.set_access_token(access_token);
            response.set_gateway_ticket(gateway_ticket);
            response.set_access_token_expires_at_epoch_millis(access_expires_at);
            response.set_gateway_ticket_expires_at_epoch_millis(
                gateway_ticket_expires_at);

            return handler::HandlerResult<
                mmo::ss::GatewayAuthLoginResponse>::success(
                    std::move(response));
        });
}

}  // namespace apps::auth_server

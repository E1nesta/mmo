#include <memory>

#include "internal/gateway_auth.pb.h"
#include "modules/auth/auth_service.h"
#include "modules/auth/mysql_account_repository.h"
#include "modules/auth/mysql_player_identity_repository.h"
#include "runtime/foundation/server_app.h"
#include "runtime/observability/logging.h"
#include "runtime/protocol/auth_tokens.h"
#include "runtime/protocol/envelope_utils.h"
#include "runtime/protocol/internal_auth.h"
#include "runtime/protocol/message_types.h"
#include "runtime/rpc/rpc_server.h"
#include "runtime/storage/storage_bootstrap.h"
#include "runtime/transport/envelope_transport.h"
#include "runtime/transport/tcp_envelope_server.h"

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

int main() {
    mmo::runtime::foundation::ServerApp app("auth_server");
    const auto tcp_options =
        mmo::runtime::transport::make_transport_options(
            app.config().transport.tcp, app.config().execution);

    std::shared_ptr<mmo::runtime::storage::MysqlConnectionPool> mysql_pool;
    std::string storage_error;
    if (!mmo::runtime::storage::initialize_mysql_pool(
            app.config(), &mysql_pool, &storage_error)) {
        mmo::runtime::observability::log_error(
            mmo::runtime::observability::LogContext{app.service_name()},
            "mysql_pool_init_failed error=" + storage_error);
        return 1;
    }

    auto account_repository =
        std::make_shared<mmo::modules::auth::MysqlAccountRepository>(mysql_pool);
    auto identity_repository =
        std::make_shared<mmo::modules::auth::MysqlPlayerIdentityRepository>(
            mysql_pool);
    mmo::modules::auth::AuthService service(
        account_repository, identity_repository);
    mmo::runtime::rpc::RpcServer rpc_server(
        mmo::runtime::rpc::make_rpc_server_options(app.config()));

    rpc_server.on(
        mmo::runtime::protocol::kGatewayAuthLoginRequest,
        [&service, &app](const mmo::common::Envelope& envelope) {
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
                        app.service_name(), envelope);
                mmo::runtime::observability::log_warn(
                    log_context,
                    "login_rejected reason=" + login.internal_reason);
                return mmo::runtime::protocol::make_error_envelope(
                    envelope, login.error_code, login.error_message);
            }
            const auto now_millis = mmo::runtime::protocol::current_time_millis();
            const auto& ticket_config = app.config().security.gateway_ticket;
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

    mmo::runtime::transport::TcpEnvelopeServer server(
        app.service_config().tcp_port,
        rpc_server.handler(),
        app.service_name(),
        tcp_options);

    mmo::runtime::observability::log_info(
        mmo::runtime::observability::LogContext{app.service_name()},
        "service_starting");
    return server.run();
}

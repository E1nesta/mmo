#include "apps/game_gateway_server/gateway_auth_proxy_handlers.h"

#include <utility>

#include "internal/gateway_auth.pb.h"
#include "public/auth.pb.h"
#include "runtime/gateway/proxy_handler.h"
#include "runtime/protocol/envelope_utils.h"
#include "apps/protocol/message_types.h"

namespace apps::game_gateway_server {

namespace app_proto = apps::protocol;
namespace gateway = runtime::gateway;
namespace protocol = runtime::protocol;

void register_gateway_auth_proxy_handlers(
    gateway::GatewayRouter& gateway_router,
    gateway::ProxyContext& context) {
    gateway::ProxyMapper<
        mmo::public_api::LoginRequest,
        mmo::internal_api::GatewayAuthLoginRequest,
        mmo::internal_api::GatewayAuthLoginResponse,
        mmo::public_api::LoginResponse>
        mapper;
    mapper.public_response_message_type =
        app_proto::kLoginResponse;
    mapper.invalid_public_request_message = "invalid login request";
    mapper.invalid_internal_response_message = "invalid auth response";
    mapper.map_request = [](const mmo::public_api::LoginRequest& request) {
        mmo::internal_api::GatewayAuthLoginRequest internal_request;
        *internal_request.mutable_context() = request.context();
        internal_request.set_account_name(request.account_name());
        internal_request.set_password(request.password());
        internal_request.set_device_id(request.device_id());
        return internal_request;
    };
    mapper.map_response = [](
                              const mmo::public_api::LoginRequest& request,
                              const mmo::internal_api::GatewayAuthLoginResponse&
                                  internal_response) {
        mmo::public_api::LoginResponse response;
        *response.mutable_context() =
            protocol::make_ok_context(request.context());
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
        return response;
    };

    gateway::bind_proxy_handler<
        mmo::public_api::LoginRequest,
        mmo::internal_api::GatewayAuthLoginRequest,
        mmo::internal_api::GatewayAuthLoginResponse,
        mmo::public_api::LoginResponse>(
        gateway_router,
        app_proto::kLoginRequest,
        context,
        std::move(mapper));
}

}  // namespace apps::game_gateway_server

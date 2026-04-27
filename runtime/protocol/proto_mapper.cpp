// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/protocol/proto_mapper.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <chrono>

namespace common::net {

namespace {

std::string ToHex(const unsigned char* data, std::size_t size) {
    static constexpr char kHexDigits[] = "0123456789abcdef";

    std::string output;
    output.reserve(size * 2);
    for (std::size_t index = 0; index < size; ++index) {
        output.push_back(kHexDigits[(data[index] >> 4U) & 0x0FU]);
        output.push_back(kHexDigits[data[index] & 0x0FU]);
    }
    return output;
}

std::optional<std::string> ComputeHmacSha256(MessageId message_id,
                                             std::uint64_t request_id,
                                             const std::string& body,
                                             const std::string& shared_secret) {
    std::string payload;
    payload.reserve(body.size() + 64);
    payload.append(std::to_string(static_cast<std::uint32_t>(message_id)));
    payload.push_back(':');
    payload.append(std::to_string(request_id));
    payload.push_back(':');
    payload.append(body);

    unsigned int digest_length = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];
    if (HMAC(EVP_sha256(),
             shared_secret.data(),
             static_cast<int>(shared_secret.size()),
             reinterpret_cast<const unsigned char*>(payload.data()),
             payload.size(),
             digest,
             &digest_length) == nullptr) {
        return std::nullopt;
    }

    return ToHex(digest, digest_length);
}

bool IsWithinClockSkew(std::int64_t timestamp_ms, std::int64_t max_clock_skew_ms) {
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    const auto delta = now_ms.count() - timestamp_ms;
    return std::llabs(delta) <= max_clock_skew_ms;
}

bool TimingSafeEqual(const std::string& left, const std::string& right) {
    if (left.size() != right.size()) {
        return false;
    }
    return CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}

template <typename ProtoRequest, typename MutateFn>
bool RewriteProtoRequest(MessageId message_id,
                         Packet* packet,
                         MutateFn&& mutate,
                         std::string* error_message = nullptr) {
    ProtoRequest request;
    if (!ParseMessage(packet->body, &request)) {
        if (error_message != nullptr) {
            *error_message = "failed to parse request packet";
        }
        return false;
    }

    mutate(request.mutable_context(), &request);
    *packet = BuildPacket(message_id, packet->header.request_id, request);
    return true;
}

template <typename ProtoRequest>
bool SignProtoRequest(MessageId message_id,
                      std::int64_t gateway_timestamp_ms,
                      const std::string& shared_secret,
                      Packet* packet,
                      std::string* error_message) {
    ProtoRequest request;
    if (!ParseMessage(packet->body, &request)) {
        if (error_message != nullptr) {
            *error_message = "failed to parse request packet";
        }
        return false;
    }

    auto* context = request.mutable_context();
    context->set_gateway_timestamp_ms(gateway_timestamp_ms);
    context->clear_gateway_signature();

    std::string unsigned_body;
    request.SerializeToString(&unsigned_body);
    const auto signature = ComputeHmacSha256(message_id, packet->header.request_id, unsigned_body, shared_secret);
    if (!signature.has_value()) {
        if (error_message != nullptr) {
            *error_message = "failed to compute gateway signature";
        }
        return false;
    }

    context->set_gateway_signature(*signature);
    *packet = BuildPacket(message_id, packet->header.request_id, request);
    return true;
}

template <typename ProtoRequest>
bool ValidateProtoRequest(MessageId message_id,
                          std::int64_t max_clock_skew_ms,
                          const std::string& shared_secret,
                          const Packet& packet,
                          std::string* error_message) {
    ProtoRequest request;
    if (!ParseMessage(packet.body, &request)) {
        if (error_message != nullptr) {
            *error_message = "failed to parse request packet";
        }
        return false;
    }

    const auto gateway_timestamp_ms = request.context().gateway_timestamp_ms();
    if (gateway_timestamp_ms <= 0) {
        if (error_message != nullptr) {
            *error_message = "missing gateway timestamp";
        }
        return false;
    }

    if (!IsWithinClockSkew(gateway_timestamp_ms, max_clock_skew_ms)) {
        if (error_message != nullptr) {
            *error_message = "gateway timestamp out of range";
        }
        return false;
    }

    const auto signature = request.context().gateway_signature();
    if (signature.empty()) {
        if (error_message != nullptr) {
            *error_message = "missing gateway signature";
        }
        return false;
    }

    request.mutable_context()->clear_gateway_signature();
    std::string unsigned_body;
    request.SerializeToString(&unsigned_body);
    const auto expected = ComputeHmacSha256(message_id, packet.header.request_id, unsigned_body, shared_secret);
    if (!expected.has_value() || !TimingSafeEqual(signature, *expected)) {
        if (error_message != nullptr) {
            *error_message = "invalid gateway signature";
        }
        return false;
    }

    return true;
}

}  // namespace

template <typename ProtoResponse>
bool ExtractProtoResponseContext(const std::string& body, RequestContext* context) {
    if (context == nullptr) {
        return false;
    }

    ProtoResponse response;
    if (!ParseMessage(body, &response)) {
        return false;
    }
    *context = FromProto(response.context());
    return true;
}

RequestContext FromProto(const game_backend::proto::RequestContext& context) {
    RequestContext output;
    output.trace_id = context.trace_id();
    output.request_id = context.request_id();
    output.auth_token = context.auth_token();
    output.player_id = context.player_id();
    output.account_id = context.account_id();
    output.gateway_timestamp_ms = context.gateway_timestamp_ms();
    output.gateway_signature = context.gateway_signature();
    return output;
}

void FillProto(const RequestContext& context, game_backend::proto::RequestContext* output) {
    if (output == nullptr) {
        return;
    }

    output->set_trace_id(context.trace_id);
    output->set_request_id(context.request_id);
    output->set_auth_token(context.auth_token);
    output->set_player_id(context.player_id);
    output->set_account_id(context.account_id);
    output->set_gateway_timestamp_ms(context.gateway_timestamp_ms);
    output->set_gateway_signature(context.gateway_signature);
}

void FillProto(const RequestContext& context, game_backend::proto::ResponseContext* output) {
    if (output == nullptr) {
        return;
    }

    output->set_trace_id(context.trace_id);
    output->set_request_id(context.request_id);
    output->set_auth_token(context.auth_token);
    output->set_player_id(context.player_id);
    output->set_account_id(context.account_id);
}

RequestContext FromProto(const game_backend::proto::ResponseContext& context) {
    RequestContext output;
    output.trace_id = context.trace_id();
    output.request_id = context.request_id();
    output.auth_token = context.auth_token();
    output.player_id = context.player_id();
    output.account_id = context.account_id();
    return output;
}

bool RewriteRequestContext(MessageId message_id, const RequestContext& context, Packet* packet) {
    if (packet == nullptr) {
        return false;
    }

    switch (message_id) {
    case MessageId::kGatePingRequest: {
        return RewriteProtoRequest<game_backend::proto::PingRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context, game_backend::proto::PingRequest*) {
                FillProto(context, proto_context);
            });
    }
    case MessageId::kAuthLoginRequest: {
        return RewriteProtoRequest<game_backend::proto::LoginRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context, game_backend::proto::LoginRequest*) {
                FillProto(context, proto_context);
            });
    }
    case MessageId::kPlayerInitRequest: {
        return RewriteProtoRequest<game_backend::proto::LoadPlayerRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context, game_backend::proto::LoadPlayerRequest*) {
                FillProto(context, proto_context);
            });
    }
    case MessageId::kPlayerSnapshotRequest: {
        return RewriteProtoRequest<game_backend::proto::PlayerSnapshotRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context,
                       game_backend::proto::PlayerSnapshotRequest*) {
                FillProto(context, proto_context);
            });
    }
    case MessageId::kEnterDungeonRequest: {
        return RewriteProtoRequest<game_backend::proto::EnterDungeonRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context, game_backend::proto::EnterDungeonRequest*) {
                FillProto(context, proto_context);
            });
    }
    case MessageId::kSettleDungeonRequest: {
        return RewriteProtoRequest<game_backend::proto::SettleDungeonRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context, game_backend::proto::SettleDungeonRequest*) {
                FillProto(context, proto_context);
            });
    }
    case MessageId::kGetActiveDungeonRequest: {
        return RewriteProtoRequest<game_backend::proto::GetActiveDungeonRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context, game_backend::proto::GetActiveDungeonRequest*) {
                FillProto(context, proto_context);
            });
    }
    case MessageId::kGetSettlementGrantStatusRequest: {
        return RewriteProtoRequest<game_backend::proto::GetSettlementGrantStatusRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context,
                       game_backend::proto::GetSettlementGrantStatusRequest*) {
                FillProto(context, proto_context);
            });
    }
    case MessageId::kListFriendsRequest: {
        return RewriteProtoRequest<game_backend::proto::ListFriendsRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context,
                       game_backend::proto::ListFriendsRequest*) { FillProto(context, proto_context); });
    }
    case MessageId::kListConversationsRequest: {
        return RewriteProtoRequest<game_backend::proto::ListConversationsRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context,
                       game_backend::proto::ListConversationsRequest*) { FillProto(context, proto_context); });
    }
    case MessageId::kGetChatHistoryRequest: {
        return RewriteProtoRequest<game_backend::proto::GetChatHistoryRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context,
                       game_backend::proto::GetChatHistoryRequest*) { FillProto(context, proto_context); });
    }
    case MessageId::kGateLoginRequest: {
        return RewriteProtoRequest<game_backend::proto::GateLoginRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context, game_backend::proto::GateLoginRequest*) {
                FillProto(context, proto_context);
            });
    }
    case MessageId::kGateRelinkRequest: {
        return RewriteProtoRequest<game_backend::proto::GateRelinkRequest>(
            message_id,
            packet,
            [&context](game_backend::proto::RequestContext* proto_context, game_backend::proto::GateRelinkRequest*) {
                FillProto(context, proto_context);
            });
    }
    default:
        return false;
    }
}

bool SignTrustedRequest(MessageId message_id,
                        std::int64_t gateway_timestamp_ms,
                        const std::string& shared_secret,
                        Packet* packet,
                        std::string* error_message) {
    if (packet == nullptr) {
        if (error_message != nullptr) {
            *error_message = "request packet is null";
        }
        return false;
    }

    switch (message_id) {
    case MessageId::kAuthLoginRequest:
        return SignProtoRequest<game_backend::proto::LoginRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    case MessageId::kPlayerInitRequest:
        return SignProtoRequest<game_backend::proto::LoadPlayerRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    case MessageId::kPlayerSnapshotRequest:
        return SignProtoRequest<game_backend::proto::PlayerSnapshotRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    case MessageId::kEnterDungeonRequest:
        return SignProtoRequest<game_backend::proto::EnterDungeonRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    case MessageId::kSettleDungeonRequest:
        return SignProtoRequest<game_backend::proto::SettleDungeonRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    case MessageId::kGetActiveDungeonRequest:
        return SignProtoRequest<game_backend::proto::GetActiveDungeonRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    case MessageId::kGetSettlementGrantStatusRequest:
        return SignProtoRequest<game_backend::proto::GetSettlementGrantStatusRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    case MessageId::kListFriendsRequest:
        return SignProtoRequest<game_backend::proto::ListFriendsRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    case MessageId::kListConversationsRequest:
        return SignProtoRequest<game_backend::proto::ListConversationsRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    case MessageId::kGetChatHistoryRequest:
        return SignProtoRequest<game_backend::proto::GetChatHistoryRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    case MessageId::kPublishGateNotificationRequest:
        return SignProtoRequest<game_backend::proto::PublishGateNotificationRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    case MessageId::kKickPlayerRequest:
        return SignProtoRequest<game_backend::proto::KickPlayerRequest>(
            message_id, gateway_timestamp_ms, shared_secret, packet, error_message);
    default:
        if (error_message != nullptr) {
            *error_message = "message does not support trusted signing";
        }
        return false;
    }
}

bool ValidateTrustedRequest(MessageId message_id,
                            std::int64_t max_clock_skew_ms,
                            const std::string& shared_secret,
                            const Packet& packet,
                            std::string* error_message) {
    switch (message_id) {
    case MessageId::kAuthLoginRequest:
        return ValidateProtoRequest<game_backend::proto::LoginRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    case MessageId::kPlayerInitRequest:
        return ValidateProtoRequest<game_backend::proto::LoadPlayerRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    case MessageId::kPlayerSnapshotRequest:
        return ValidateProtoRequest<game_backend::proto::PlayerSnapshotRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    case MessageId::kEnterDungeonRequest:
        return ValidateProtoRequest<game_backend::proto::EnterDungeonRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    case MessageId::kSettleDungeonRequest:
        return ValidateProtoRequest<game_backend::proto::SettleDungeonRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    case MessageId::kGetActiveDungeonRequest:
        return ValidateProtoRequest<game_backend::proto::GetActiveDungeonRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    case MessageId::kGetSettlementGrantStatusRequest:
        return ValidateProtoRequest<game_backend::proto::GetSettlementGrantStatusRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    case MessageId::kListFriendsRequest:
        return ValidateProtoRequest<game_backend::proto::ListFriendsRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    case MessageId::kListConversationsRequest:
        return ValidateProtoRequest<game_backend::proto::ListConversationsRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    case MessageId::kGetChatHistoryRequest:
        return ValidateProtoRequest<game_backend::proto::GetChatHistoryRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    case MessageId::kPublishGateNotificationRequest:
        return ValidateProtoRequest<game_backend::proto::PublishGateNotificationRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    case MessageId::kKickPlayerRequest:
        return ValidateProtoRequest<game_backend::proto::KickPlayerRequest>(
            message_id, max_clock_skew_ms, shared_secret, packet, error_message);
    default:
        if (error_message != nullptr) {
            *error_message = "message does not require trusted gateway validation";
        }
        return false;
    }
}

Packet BuildErrorPacket(const RequestContext& context,
                        common::error::ErrorCode error_code,
                        const std::string& error_message) {
    game_backend::proto::ErrorResponse response;
    FillProto(context, response.mutable_context());
    response.set_error_code(static_cast<int>(error_code));
    response.set_error_name(std::string(common::error::ToString(error_code)));
    response.set_error_message(error_message);
    return BuildPacket(MessageId::kErrorResponse, context.request_id, response);
}

Packet BuildPingResponsePacket(const RequestContext& context, const std::string& message) {
    game_backend::proto::PingResponse response;
    FillProto(context, response.mutable_context());
    response.set_message(message);
    return BuildPacket(MessageId::kGatePongResponse, context.request_id, response);
}

Packet BuildGateKickPacket(const RequestContext& context, const std::string& message) {
    game_backend::proto::GateKick response;
    FillProto(context, response.mutable_context());
    response.set_message(message);
    return BuildPacket(MessageId::kGateKick, context.request_id, response);
}

Packet BuildGateNotificationPacket(const RequestContext& context, int type, std::int64_t timestamp) {
    game_backend::proto::GateNotification response;
    FillProto(context, response.mutable_context());
    response.set_type(type);
    response.set_timestamp(timestamp);
    return BuildPacket(MessageId::kGateNotification, context.request_id, response);
}

bool ExtractRequestContext(MessageId message_id, const std::string& body, RequestContext* context) {
    if (context == nullptr) {
        return false;
    }

    switch (message_id) {
    case MessageId::kGatePingRequest: {
        game_backend::proto::PingRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kAuthLoginRequest: {
        game_backend::proto::LoginRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kPlayerInitRequest: {
        game_backend::proto::LoadPlayerRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kPlayerSnapshotRequest: {
        game_backend::proto::PlayerSnapshotRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kEnterDungeonRequest: {
        game_backend::proto::EnterDungeonRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kSettleDungeonRequest: {
        game_backend::proto::SettleDungeonRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kGetActiveDungeonRequest: {
        game_backend::proto::GetActiveDungeonRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kGetSettlementGrantStatusRequest: {
        game_backend::proto::GetSettlementGrantStatusRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kListFriendsRequest: {
        game_backend::proto::ListFriendsRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kListConversationsRequest: {
        game_backend::proto::ListConversationsRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kGetChatHistoryRequest: {
        game_backend::proto::GetChatHistoryRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kGateLoginRequest: {
        game_backend::proto::GateLoginRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kGateRelinkRequest: {
        game_backend::proto::GateRelinkRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kPublishGateNotificationRequest: {
        game_backend::proto::PublishGateNotificationRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    case MessageId::kKickPlayerRequest: {
        game_backend::proto::KickPlayerRequest request;
        return ParseMessage(body, &request) && (*context = FromProto(request.context()), true);
    }
    default:
        return false;
    }
}

bool ExtractResponseContext(MessageId message_id, const std::string& body, RequestContext* context) {
    switch (message_id) {
    case MessageId::kGatePongResponse:
        return ExtractProtoResponseContext<game_backend::proto::PingResponse>(body, context);
    case MessageId::kErrorResponse:
        return ExtractProtoResponseContext<game_backend::proto::ErrorResponse>(body, context);
    case MessageId::kAuthLoginResponse:
        return ExtractProtoResponseContext<game_backend::proto::LoginResponse>(body, context);
    case MessageId::kPlayerInitResponse:
        return ExtractProtoResponseContext<game_backend::proto::LoadPlayerResponse>(body, context);
    case MessageId::kPlayerSnapshotResponse:
        return ExtractProtoResponseContext<game_backend::proto::PlayerSnapshotResponse>(body, context);
    case MessageId::kEnterDungeonResponse:
        return ExtractProtoResponseContext<game_backend::proto::EnterDungeonResponse>(body, context);
    case MessageId::kSettleDungeonResponse:
        return ExtractProtoResponseContext<game_backend::proto::SettleDungeonResponse>(body, context);
    case MessageId::kGetSettlementGrantStatusResponse:
        return ExtractProtoResponseContext<game_backend::proto::GetSettlementGrantStatusResponse>(body, context);
    case MessageId::kGetActiveDungeonResponse:
        return ExtractProtoResponseContext<game_backend::proto::GetActiveDungeonResponse>(body, context);
    case MessageId::kListFriendsResponse:
        return ExtractProtoResponseContext<game_backend::proto::ListFriendsResponse>(body, context);
    case MessageId::kListConversationsResponse:
        return ExtractProtoResponseContext<game_backend::proto::ListConversationsResponse>(body, context);
    case MessageId::kGetChatHistoryResponse:
        return ExtractProtoResponseContext<game_backend::proto::GetChatHistoryResponse>(body, context);
    case MessageId::kGateLoginResponse:
        return ExtractProtoResponseContext<game_backend::proto::GateLoginResponse>(body, context);
    case MessageId::kGateRelinkResponse:
        return ExtractProtoResponseContext<game_backend::proto::GateRelinkResponse>(body, context);
    case MessageId::kSocialFriendNotification:
        return ExtractProtoResponseContext<game_backend::proto::SocialFriendNotification>(body, context);
    case MessageId::kSocialChatPush:
        return ExtractProtoResponseContext<game_backend::proto::SocialChatPush>(body, context);
    case MessageId::kGateKick:
        return ExtractProtoResponseContext<game_backend::proto::GateKick>(body, context);
    case MessageId::kGateNotification:
        return ExtractProtoResponseContext<game_backend::proto::GateNotification>(body, context);
    case MessageId::kPublishGateNotificationResponse:
        return ExtractProtoResponseContext<game_backend::proto::PublishGateNotificationResponse>(body, context);
    case MessageId::kKickPlayerResponse:
        return ExtractProtoResponseContext<game_backend::proto::KickPlayerResponse>(body, context);
    default:
        return false;
    }
}

}  // namespace common::net

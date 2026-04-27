// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/error/error_code.h"
#include "runtime/session/session.h"
#include "runtime/protocol/message_id.h"
#include "runtime/protocol/proto_codec.h"
#include "runtime/protocol/request_context.h"

#include "game_backend.pb.h"

#include <optional>
#include <string>

namespace common::net {

RequestContext FromProto(const game_backend::proto::RequestContext& context);
void FillProto(const RequestContext& context, game_backend::proto::RequestContext* output);
void FillProto(const RequestContext& context, game_backend::proto::ResponseContext* output);
RequestContext FromProto(const game_backend::proto::ResponseContext& context);
bool RewriteRequestContext(MessageId message_id, const RequestContext& context, Packet* packet);
bool SignTrustedRequest(MessageId message_id,
                        std::int64_t gateway_timestamp_ms,
                        const std::string& shared_secret,
                        Packet* packet,
                        std::string* error_message);
bool ValidateTrustedRequest(MessageId message_id,
                            std::int64_t max_clock_skew_ms,
                            const std::string& shared_secret,
                            const Packet& packet,
                            std::string* error_message);

Packet BuildErrorPacket(const RequestContext& context,
                        common::error::ErrorCode error_code,
                        const std::string& error_message);
Packet BuildPingResponsePacket(const RequestContext& context, const std::string& message);
Packet BuildGateKickPacket(const RequestContext& context, const std::string& message);
Packet BuildGateNotificationPacket(const RequestContext& context, int type, std::int64_t timestamp);

bool ExtractRequestContext(MessageId message_id, const std::string& body, RequestContext* context);
bool ExtractResponseContext(MessageId message_id, const std::string& body, RequestContext* context);

}  // namespace common::net

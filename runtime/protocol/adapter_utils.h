// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/foundation/error/error_code.h"
#include "runtime/protocol/message_id.h"
#include "runtime/protocol/packet.h"
#include "runtime/protocol/proto_codec.h"
#include "runtime/protocol/proto_mapper.h"
#include "runtime/protocol/error_responder.h"
#include "runtime/protocol/handler_context.h"

#include <string>
#include <utility>

namespace framework::protocol {

// 共享适配辅助工具让入口保持解析、处理、组包三段清晰。
template <typename ProtoRequest>
bool ParseProtoRequest(const HandlerContext& context,
                       const common::net::Packet& packet,
                       const std::string& invalid_message,
                       ProtoRequest* request,
                       common::net::Packet* error_response) {
    if (request != nullptr && common::net::ParseMessage(packet.body, request)) {
        return true;
    }

    if (error_response != nullptr) {
        *error_response =
            BuildErrorResponse(context.request, common::error::ErrorCode::kRequestContextInvalid, invalid_message);
    }
    return false;
}

template <typename ProtoResponse>
void FillResponseContext(const common::net::RequestContext& response_context, ProtoResponse* response) {
    if (response == nullptr) {
        return;
    }

    common::net::FillProto(response_context, response->mutable_context());
}

template <typename ProtoRequest, typename InvokeFn, typename BuildResponseFn>
common::net::Packet HandleParsedRequest(const HandlerContext& context,
                                        const common::net::Packet& packet,
                                        const std::string& invalid_message,
                                        InvokeFn&& invoke,
                                        BuildResponseFn&& build_response) {
    ProtoRequest request;
    common::net::Packet error_response;
    if (!ParseProtoRequest(context, packet, invalid_message, &request, &error_response)) {
        return error_response;
    }

    auto result = std::forward<InvokeFn>(invoke)(request);
    if (!result.success) {
        return BuildErrorResponse(context.request, result.error_code, result.error_message);
    }
    return std::forward<BuildResponseFn>(build_response)(context, result);
}

}  // namespace framework::protocol

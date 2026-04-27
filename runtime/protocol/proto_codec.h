// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/protocol/message_id.h"
#include "runtime/protocol/packet.h"

#include <google/protobuf/message_lite.h>

#include <string>

namespace common::net {

template <typename MessageT>
bool ParseMessage(const std::string& body, MessageT* message) {
    return message != nullptr && message->ParseFromString(body);
}

template <typename MessageT>
Packet BuildPacket(MessageId message_id, std::uint64_t request_id, const MessageT& message) {
    Packet packet;
    packet.header.msg_id = static_cast<std::uint32_t>(message_id);
    packet.header.request_id = request_id;
    message.SerializeToString(&packet.body);
    packet.header.body_len = static_cast<std::uint32_t>(packet.body.size());
    return packet;
}

}  // namespace common::net

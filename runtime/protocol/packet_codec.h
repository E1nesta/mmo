// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/protocol/packet.h"

#include <string>
#include <string_view>

namespace framework::protocol {

constexpr std::size_t kPacketHeaderSize = common::net::kPacketHeaderSize;
constexpr std::uint32_t kDefaultMaxPacketBodyBytes = 4U * 1024U * 1024U;

std::string EncodePacket(const common::net::Packet& packet);
bool DecodeHeader(std::string_view bytes,
                  common::net::PacketHeader* header,
                  std::string* error_message = nullptr,
                  std::uint32_t max_body_bytes = kDefaultMaxPacketBodyBytes);
bool TryExtractPacket(std::string& buffer, common::net::Packet* packet);

}  // namespace framework::protocol

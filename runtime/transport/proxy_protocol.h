// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace framework::transport {

constexpr std::size_t kMaxProxyProtocolHeaderBytes = 108;

// 解析带 CRLF 的 PROXY protocol v1 头行。
// 解析成功返回 true；遇到 PROXY UNKNOWN 时清空 peer_address。
bool ParseProxyProtocolHeader(std::string_view header_line,
                              std::string* peer_address,
                              std::string* error_message = nullptr);

}  // namespace framework::transport

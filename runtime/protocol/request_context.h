// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <cstdint>
#include <string>

namespace common::net {

struct RequestContext {
    std::string trace_id;
    std::uint64_t request_id = 0;
    std::string auth_token;
    std::int64_t player_id = 0;
    std::int64_t account_id = 0;
    std::int64_t gateway_timestamp_ms = 0;
    std::string gateway_signature;
};

}  // namespace common::net

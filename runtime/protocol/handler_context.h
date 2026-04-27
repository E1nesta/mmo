// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/protocol/message_id.h"
#include "runtime/protocol/request_context.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace framework::protocol {

struct HandlerContext {
    common::net::RequestContext request;
    common::net::MessageId message_id = common::net::MessageId::kPingRequest;
    std::uint64_t connection_id = 0;
    std::string peer_address;
    std::optional<std::size_t> executor_shard;
    std::string executor_label;
};

HandlerContext NormalizeContext(const std::string& trace_prefix,
                               common::net::MessageId message_id,
                               std::uint64_t connection_id,
                               const std::string& peer_address,
                               std::uint64_t request_id,
                               const common::net::RequestContext& parsed);

}  // namespace framework::protocol

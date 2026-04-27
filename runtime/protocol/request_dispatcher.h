// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/protocol/message_id.h"
#include "runtime/protocol/packet.h"
#include "runtime/protocol/handler_context.h"

#include <functional>
#include <optional>
#include <unordered_map>

namespace framework::protocol {

using RouteHandler = std::function<common::net::Packet(const HandlerContext&, const common::net::Packet&)>;

class RequestDispatcher {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    void Register(common::net::MessageId message_id, RouteHandler handler);
    [[nodiscard]] bool CanHandle(common::net::MessageId message_id) const;
    [[nodiscard]] std::optional<common::net::Packet> Dispatch(common::net::MessageId message_id,
                                                              const HandlerContext& context,
                                                              const common::net::Packet& packet) const;

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    std::unordered_map<std::uint32_t, RouteHandler> handlers_;
};

}  // namespace framework::protocol

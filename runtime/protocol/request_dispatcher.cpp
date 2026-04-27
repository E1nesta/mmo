// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/protocol/request_dispatcher.h"

namespace framework::protocol {

void RequestDispatcher::Register(common::net::MessageId message_id, RouteHandler handler) {
    handlers_[static_cast<std::uint32_t>(message_id)] = std::move(handler);
}

bool RequestDispatcher::CanHandle(common::net::MessageId message_id) const {
    return handlers_.find(static_cast<std::uint32_t>(message_id)) != handlers_.end();
}

// 请求处理：`Dispatch` 承接边界输入并转发到目标链路。
std::optional<common::net::Packet> RequestDispatcher::Dispatch(common::net::MessageId message_id,
                                                               const HandlerContext& context,
                                                               const common::net::Packet& packet) const {
    const auto iter = handlers_.find(static_cast<std::uint32_t>(message_id));
    if (iter == handlers_.end()) {
        return std::nullopt;
    }

    return iter->second(context, packet);
}

}  // namespace framework::protocol

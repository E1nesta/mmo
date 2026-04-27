// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/protocol/request_dispatcher.h"

namespace framework::service {

class RouteRegistry {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    void Register(common::net::MessageId message_id, framework::protocol::RouteHandler handler) {
        dispatcher_.Register(message_id, std::move(handler));
    }

    [[nodiscard]] const framework::protocol::RequestDispatcher& Dispatcher() const {
        return dispatcher_;
    }

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    framework::protocol::RequestDispatcher dispatcher_;
};

}  // namespace framework::service

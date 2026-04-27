// 代码规范落地：运行时通用层实现文件，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#include "runtime/http/http_router.h"

namespace framework::http {

void HttpRouter::RegisterPost(std::string path, HttpHandler handler) {
    std::lock_guard lock(mutex_);
    post_routes_[std::move(path)] = std::move(handler);
}

// 请求处理：`Handle` 承接边界输入并转发到目标链路。
HttpResponse HttpRouter::Handle(const HttpRequest& request) const {
    std::lock_guard lock(mutex_);
    if (request.method != "POST") {
        return {405, "text/plain", "method not allowed", {}};
    }

    const auto iter = post_routes_.find(request.target);
    if (iter == post_routes_.end()) {
        return {404, "text/plain", "not found", {}};
    }
    return iter->second(request);
}

}  // namespace framework::http

// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace framework::http {

struct HttpRequest {
    std::string method;
    std::string target;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
};

struct HttpResponse {
    unsigned status = 200;
    std::string content_type = "application/octet-stream";
    std::string body;
    std::unordered_map<std::string, std::string> headers;
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

}  // namespace framework::http

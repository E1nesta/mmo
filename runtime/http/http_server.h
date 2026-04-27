// 代码规范落地：运行时通用层接口定义，保持职责边界与主流程可读。
// 分段约定：前置判断 -> 输入准备 -> 核心处理 -> 结果收敛 -> 状态更新 -> 返回出口。

#pragma once

#include "runtime/http/http_router.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace framework::http {

class HttpServer {
// 对外接口：保持入口语义清晰，避免在声明层混入实现细节。
public:
    HttpServer(std::string host, int port);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    bool Start(HttpRouter* router, std::string* error_message);
    void Wait();
    void Shutdown();

// 内部实现：收敛辅助能力与状态字段，避免边界泄漏。
private:
    void Run();

    std::string host_;
    int port_ = 0;
    HttpRouter* router_ = nullptr;
    std::atomic_bool running_{false};
    std::unique_ptr<std::thread> thread_;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace framework::http
